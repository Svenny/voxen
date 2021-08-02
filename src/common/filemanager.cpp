#include <voxen/common/filemanager.hpp>

#include <fstream>
#include <cassert>
#include <atomic>
#include <vector>
#include <queue>
#include <thread>
#include <future>
#include <functional>

#include <voxen/util/log.hpp>
#include <voxen/config.hpp>
#include <extras/defer.hpp>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sago/platform_folders.h>

using std::filesystem::path;
using std::ifstream;
using std::ofstream;
using std::optional;
using std::string;
using std::vector;
using std::future;
using std::thread;
using std::packaged_task;
using std::atomic_bool;
using extras::dyn_array;

using namespace std::chrono_literals;

namespace
{

static void printErrno(const char *message, const string& path) noexcept {
	int code = errno;
	char buf[1024];
	char *desc = strerror_r(code, buf, std::size(buf));
	voxen::Log::error("{} `{}`, error code {} ({})", message, path, code, desc);
}

struct RawBytesStorage {
	void set_size(size_t size) {
		std::allocator<std::byte> alloc;
		m_data = alloc.allocate(size);
		m_size = size;
	}
	void* data() noexcept { return m_data; }

	std::byte* m_data = nullptr;
	size_t m_size = 0UL;
};

struct StringStorage {
	void set_size(size_t size) noexcept {
		m_string.resize(size, ' ');
	}
	void* data() noexcept { return m_string.data(); }

	string m_string;
};

template<typename T>
bool readAbsPath(const string& path, T& storageObject) noexcept {
	// TODO: handle EINTR
	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		printErrno("Can't open file", path);
		return false;
	}
	defer { close(fd); };

	struct stat file_stat;
	if (fstat(fd, &file_stat) != 0) {
		printErrno("Can't stat file", path);
		return false;
	}

	size_t size = size_t(file_stat.st_size);
	try {
		//NOTE(sirgienko) Maybe there is a better solution
		storageObject.set_size(size);

		// TODO: handle EINTR
		if (read(fd, storageObject.data(), size) < 0) {
			printErrno("Can't read file", path);
			return false;
		}
		return true;
	}
	catch (const std::bad_alloc &e) {
		voxen::Log::error("Out of memory: {}", e.what());
		return false;
	}
}

static bool writeAbsPathFile(const string& path, const void *data, size_t size) noexcept {
	string temp_path_s;
	try {
		temp_path_s = path;
		temp_path_s += ".XXXXXX";
	}
	catch (const std::bad_alloc &e) {
		voxen::Log::error("Out of memory: {}", e.what());
		return false;
	}

	// TODO: handle EINTR
	int fd = mkstemp(temp_path_s.data());
	const char *temp_path = temp_path_s.c_str();
	if (fd < 0) {
		printErrno("Can't mkstemp file", temp_path);
		return false;
	}
	defer { close(fd); };

	// TODO: handle EINTR
	ssize_t written = write(fd, data, size);
	if (written < 0) {
		printErrno("Can't write file", temp_path);
		return false;
	} else if (size_t(written) != size) {
		printErrno("Can't fully write file", temp_path);
		voxen::Log::error("{}/{} bytes written", written, size);
		return false;
	}

	if (rename(temp_path, path.c_str()) != 0) {
		printErrno("Can't rename file", temp_path);
		return false;
	}

	return true;
}

class FileIoThreadPool {
private:
	struct ReportableWorkerState {
		std::mutex semaphore_mutex;
		std::condition_variable semaphore;
		atomic_bool is_exit = false;

		std::mutex state_mutex;
		bool has_task;
		bool live_forever;
		std::queue<packaged_task<void()>> tasks_queue;
	};

	struct ReportableWorker {
		thread worker;
		ReportableWorkerState state;
	};

public:
	FileIoThreadPool()
	{
		for (size_t i = 0; i < START_THREAD_COUNT; i++)
			run_worker(make_worker());
	}

	~FileIoThreadPool()
	{
		for (ReportableWorker* worker : threads)
		{
			worker->state.is_exit.store(true);
			worker->state.semaphore.notify_one();
		}

		for (ReportableWorker* worker : threads)
			worker->worker.join();

		while (threads.size() > 0)
		{
			delete threads.back();
			threads.pop_back();
		}
	}

	void enqueueTask(std::function<void()>&& task_function)
	{
		voxen::Log::info("enqueueTask");
		cleanup_finished_workers();

		packaged_task<void()> task(task_function);

		for (size_t i = 0; i < threads.size(); i++)
		{
			ReportableWorker* worker = threads[i];
			worker->state.state_mutex.lock();
			if (!worker->state.has_task)
			{
				worker->state.tasks_queue.push(std::move(task));
				worker->state.has_task = true;
				worker->state.state_mutex.unlock();

				worker->state.semaphore.notify_one();
				return;
			}
			else
				worker->state.state_mutex.unlock();
		}
		// If we here, then no free threads, so construct new for the task
		// So, make worker, push task and run the worker
		ReportableWorker* new_worker = make_worker();

		new_worker->state.tasks_queue.push(std::move(task));
		new_worker->state.has_task = true;

		run_worker(new_worker);
		return;
	}

private:
	static void workerFunction(ReportableWorkerState* state)
	{
		state->state_mutex.lock();
		bool is_eternal_thread = state->live_forever;
		state->state_mutex.unlock();

		while(!state->is_exit.load())
		{
			state->state_mutex.lock();
			if (state->tasks_queue.size() == 1)
			{
				packaged_task<void()> task = std::move(state->tasks_queue.front());
				state->tasks_queue.pop();
				state->state_mutex.unlock();

				task();

				state->state_mutex.lock();
				state->has_task = false;
			}
			state->state_mutex.unlock();

			{
			std::unique_lock<std::mutex> lock(state->semaphore_mutex);
			if (is_eternal_thread)
				state->semaphore.wait(lock);
			else
			{
				std::cv_status status = state->semaphore.wait_for(lock, THREAD_LIVE_TIMEOUT);
				if (status == std::cv_status::timeout)
					state->is_exit.store(true);
			}
			}
		}
	}

	ReportableWorker* make_worker()
	{
		ReportableWorker* new_worker = new ReportableWorker();
		new_worker->state.has_task = false;
		if (threads.size() < ETERNAL_THREAD_COUNT)
			new_worker->state.live_forever = true;
		else
			new_worker->state.live_forever = false;
		threads.push_back(new_worker);
		return new_worker;
	}

	void run_worker(ReportableWorker* worker)
	{
		packaged_task<void(ReportableWorkerState*)> main_work_task(&FileIoThreadPool::workerFunction);
		worker->worker = std::thread(std::move(main_work_task), &worker->state);
	}

	void cleanup_finished_workers()
	{
		for (auto iter = threads.begin(); iter != threads.end();)
		{
			if ((*iter)->state.is_exit.load())
			{
				delete *iter;
				iter = threads.erase(iter);
			}
			else
				iter++;
		}
	}

private:
	vector<ReportableWorker*> threads;

	constexpr static std::chrono::milliseconds THREAD_LIVE_TIMEOUT = 5000ms;
	constexpr static size_t ETERNAL_THREAD_COUNT = 2;
	constexpr static size_t START_THREAD_COUNT = FileIoThreadPool::ETERNAL_THREAD_COUNT;
};

}

static FileIoThreadPool file_manager_threadpool;

namespace voxen
{

path FileManager::user_data_path;
path FileManager::game_data_path;

void FileManager::setProfileName(std::string profile_name)
{
	if constexpr (BuildConfig::kIsDeployBuild) {
		assert(false);
		user_data_path = std::filesystem::path(sago::getDataHome()) / "voxen/";
	} else {
		user_data_path = std::filesystem::current_path() / "data/user" / profile_name;
		game_data_path = std::filesystem::current_path() / "data/game";

	}
}

optional<dyn_array<std::byte>> FileManager::readUserFile(const path& relative_path) noexcept
{
	if (relative_path.empty())
		return std::nullopt;

	RawBytesStorage storage;
	bool success = readAbsPath(FileManager::userDataPath() / relative_path, storage);
	return success ? optional(dyn_array<std::byte>(storage.m_data, storage.m_size, std::allocator<std::byte>())) : std::nullopt;
}

bool FileManager::writeUserFile(const path& relative_path, const void *data, size_t size, bool create_directories) noexcept
{
	path filepath = FileManager::userDataPath() / relative_path;
	if (create_directories)
	{
		if (!FileManager::makeDirsForFile(filepath))
			return false;
	}

	return writeAbsPathFile(filepath, data, size);
}

optional<string> FileManager::readUserTextFile(const path& relative_path) noexcept
{
	if (relative_path.empty())
		return std::nullopt;

	StringStorage storage;
	bool success = readAbsPath(FileManager::userDataPath() / relative_path, storage);
	return success ? optional(storage.m_string) : std::nullopt;
}

bool FileManager::writeUserTextFile(const path& relative_path, const string& text, bool create_directories) noexcept
{
	path filepath = FileManager::userDataPath() / relative_path;
	if (create_directories)
	{
		if (!FileManager::makeDirsForFile(filepath))
			return false;
	}

	return writeAbsPathFile(filepath, text.data(), text.size());
}

optional<dyn_array<std::byte>> FileManager::readFile(const path& relative_path) noexcept
{
	if (relative_path.empty())
		return std::nullopt;

	RawBytesStorage storage;
	bool success = readAbsPath(FileManager::gameDataPath() / relative_path, storage);
	return success ? optional(dyn_array<std::byte>(storage.m_data, storage.m_size, std::allocator<std::byte>())) : std::nullopt;
}

optional<string> FileManager::readTextFile(const path& relative_path) noexcept
{
	if (relative_path.empty())
		return std::nullopt;

	StringStorage storage;
	bool success = readAbsPath(FileManager::gameDataPath() / relative_path, storage);
	return success ? optional(storage.m_string) : std::nullopt;
}

bool FileManager::makeDirsForFile(const std::filesystem::path& relative_path) noexcept
{
	try {
		std::filesystem::create_directories(relative_path.parent_path());
	}
	catch (const std::bad_alloc &e) {
		voxen::Log::error("Out of memory: {}", e.what());
		return false;
	}
	catch (const std::filesystem::filesystem_error &e) {
		voxen::Log::error("Directory creation error: {}", e.what());
		return false;
	}

	return true;
}

path FileManager::userDataPath() noexcept
{
	return user_data_path;
}

path FileManager::gameDataPath() noexcept
{
	return game_data_path;
}

std::future<std::optional<extras::dyn_array<std::byte>>> FileManager::readFileAsync(std::filesystem::path relative_path) noexcept
{
	std::shared_ptr<std::promise<optional<dyn_array<std::byte>>>> promise(new std::promise<optional<dyn_array<std::byte>>>);
	std::future<optional<dyn_array<std::byte>>> result = promise->get_future();

	std::function<void()> task_function([filapath = std::move(relative_path), p = promise]() mutable {
		p->set_value(FileManager::readFile(filapath));
	});
	file_manager_threadpool.enqueueTask(std::move(task_function));
	return result;
}

std::future<std::optional<extras::dyn_array<std::byte> > > FileManager::readUserFileAsync(std::filesystem::path relative_path) noexcept
{
	std::shared_ptr<std::promise<optional<dyn_array<std::byte>>>> promise(new std::promise<optional<dyn_array<std::byte>>>);
	std::future<optional<dyn_array<std::byte>>> result = promise->get_future();

	std::function<void()> task_function([filapath = std::move(relative_path), p = std::move(promise)]() mutable {
		p->set_value(FileManager::readUserFile(filapath));
	});
	file_manager_threadpool.enqueueTask(std::move(task_function));
	return result;
}

std::future<std::optional<std::string>> FileManager::readTextFileAsync(std::filesystem::path relative_path) noexcept
{
	std::shared_ptr<std::promise<optional<string>>> promise(new std::promise<optional<string>>);
	std::future<optional<string>> result = promise->get_future();

	std::function<void()> task_function([filapath = std::move(relative_path), p = std::move(promise)]() mutable {
		p->set_value(FileManager::readTextFile(filapath));
	});
	file_manager_threadpool.enqueueTask(std::move(task_function));
	return result;
}

std::future<std::optional<std::string>> FileManager::readUserTextFileAsync(std::filesystem::path relative_path) noexcept
{
	std::shared_ptr<std::promise<optional<string>>> promise(new std::promise<optional<string>>);
	std::future<optional<string>> result = promise->get_future();

	std::function<void()> task_function([filapath = std::move(relative_path), p = std::move(promise)]() mutable {
		p->set_value(FileManager::readUserTextFile(filapath));
	});
	file_manager_threadpool.enqueueTask(std::move(task_function));
	return result;
}

std::future<bool> FileManager::writeUserFileAsync(std::filesystem::path relative_path, const void* data, size_t size, bool create_directories) noexcept
{
	std::shared_ptr<std::promise<bool>> promise(new std::promise<bool>);
	std::future<bool> result = promise->get_future();

	std::function<void()> task_function([filapath = std::move(relative_path), p = std::move(promise), data, size, create_directories]() mutable {
		p->set_value(FileManager::writeUserFile(filapath, data, size, create_directories));
	});
	file_manager_threadpool.enqueueTask(std::move(task_function));
	return result;
}

std::future<bool> FileManager::writeUserTextFileAsync(std::filesystem::path relative_path, const std::string&& text, bool create_directories) noexcept
{
	std::shared_ptr<std::promise<bool>> promise(new std::promise<bool>);
	std::future<bool> result = promise->get_future();

	std::function<void()> task_function([filapath = std::move(relative_path), p = std::move(promise), txt = std::move(text), create_directories]() mutable {
		p->set_value(FileManager::writeUserTextFile(filapath, std::move(txt), create_directories));
	});
	file_manager_threadpool.enqueueTask(std::move(task_function));
	return result;
}


}
