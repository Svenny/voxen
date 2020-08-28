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

namespace impl {
static void printErrno(const char *message, const string& path) noexcept {
	int code = errno;
	char buf[1024];
	char *desc = strerror_r(code, buf, std::size(buf));
	voxen::Log::warn("{} `{}`, error code {} ({})", message, path, code, desc);
}

struct RawBytesStorage {
	void set_size(size_t size) noexcept {
		try {
			m_data = new std::byte[size];
			m_size = size;
		}
		catch (const std::bad_alloc &e) {
			voxen::Log::warn("Out of memory: {}", e.what());
			m_is_error = true;
		}
	}
	bool is_error() noexcept { return m_is_error; }
	void* data() noexcept { return m_data; }

	std::byte* m_data = nullptr;
	bool m_is_error = false;
	size_t m_size = 0UL;
};

struct StringStorage {
	void set_size(size_t size) noexcept {
		try {
			m_string.resize(size, ' ');
		}
		catch (const std::bad_alloc &e) {
			voxen::Log::warn("Out of memory: {}", e.what());
			m_is_error = true;
		}
	}
	bool is_error() noexcept { return m_is_error; }
	void* data() noexcept { return m_string.data(); }

	string m_string;
	bool m_is_error = false;
};

template<typename T>
bool readAbsPath(const string& path, T& storageObject) {
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
		if (storageObject.is_error())
			return false;

		// TODO: handle EINTR
		if (read(fd, storageObject.data(), size) < 0) {
			printErrno("Can't read file", path);
			return false;
		}
		return true;
	}
	catch (const std::bad_alloc &e) {
		voxen::Log::warn("Out of memory: {}", e.what());
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
		voxen::Log::warn("Out of memory: {}", e.what());
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
		voxen::Log::warn("{}/{} bytes written", written, size);
		return false;
	}

	if (rename(temp_path, path.c_str()) != 0) {
		printErrno("Can't rename file", temp_path);
		return false;
	}

	return true;
}

struct ReportableWorkerState {
	std::mutex semaphore_mutex;
	std::condition_variable semaphore;
	atomic_bool is_exit = false;

	std::mutex state_mutex;
	bool has_task;
	bool live_forever;
	std::queue<packaged_task<optional<dyn_array<std::byte>>()>> read_tasks_queue;
	std::queue<packaged_task<optional<string>()>> read_text_tasks_queue;
	std::queue<packaged_task<bool()>> write_tasks_queue;
};

struct ReportableWorker {
	thread worker;
	ReportableWorkerState state;
};

template<typename T> void runTaskFromQueue(ReportableWorkerState* state, std::queue<packaged_task<T()>>& queue)
{
	assert(queue.size() == 1);
	packaged_task<T()> task = std::move(queue.front());
	queue.pop();
	state->state_mutex.unlock();

	task();

	state->state_mutex.lock();
	state->has_task = false;
}

class FileIoThreadPool {
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

	future<optional<dyn_array<std::byte>>> enqueueFileRead(const path& relative_path, std::function<optional<dyn_array<std::byte>>(std::filesystem::path)> task_function)
	{
		return enqueueTask<optional<dyn_array<std::byte>>>(relative_path, std::move(task_function), FileIoThreadPool::enqueueReadQeueue);
	}

	future<optional<string>> enqueueFileTextRead(const path& relative_path, std::function<optional<string>(std::filesystem::path)> task_function)
	{
		return enqueueTask<optional<string>>(relative_path, std::move(task_function), FileIoThreadPool::enqueueReadTextQeueue);
	}

	future<bool> enqueueFileWrite(const path& relative_path, std::function<bool(std::filesystem::path)> task_function)
	{
		return enqueueTask<bool>(relative_path, std::move(task_function), FileIoThreadPool::enqueueWriteQeueue);
	}

private:
	static void enqueueReadQeueue(ReportableWorkerState* state, packaged_task<optional<dyn_array<std::byte>>()>& task)
	{
		state->read_tasks_queue.push(std::move(task));
	}

	static void enqueueReadTextQeueue(ReportableWorkerState* state, packaged_task<optional<string>()>& task)
	{
		state->read_text_tasks_queue.push(std::move(task));
	}

	static void enqueueWriteQeueue(ReportableWorkerState* state, packaged_task<bool()>& task)
	{
		state->write_tasks_queue.push(std::move(task));
	}

	static void workerFunction(ReportableWorkerState* state)
	{
		state->state_mutex.lock();
		bool is_eternal_thread = state->live_forever;
		state->state_mutex.unlock();

		while(!state->is_exit.load())
		{
			state->state_mutex.lock();
			if (state->read_tasks_queue.size() == 1)
			{
				runTaskFromQueue(state, state->read_tasks_queue);
			}
			if (state->read_text_tasks_queue.size() == 1)
			{
				runTaskFromQueue(state, state->read_text_tasks_queue);
			}
			else if (state->write_tasks_queue.size() == 1)
			{
				runTaskFromQueue(state, state->write_tasks_queue);
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

	template<typename T>
	future<T> enqueueTask(const path& relative_path, std::function<T(std::filesystem::path)> task_function, std::function<void(ReportableWorkerState*, packaged_task<T()>&)> task_pusher_function)
	{
		cleanup_finished_workers();

		packaged_task<T()> task(std::bind(task_function, relative_path));

		voxen::Log::info("enqueue read task to threadpool size {}", threads.size());
		for (size_t i = 0; i < threads.size(); i++)
		{
			ReportableWorker* worker = threads[i];
			worker->state.state_mutex.lock();
			if (!worker->state.has_task)
			{
				voxen::Log::info("use already created thread #{}", i);
				future<T> result = task.get_future();
				task_pusher_function(&worker->state, task);
				worker->state.has_task = true;
				worker->state.state_mutex.unlock();

				worker->state.semaphore.notify_one();
				return result;
			}
			else
				worker->state.state_mutex.unlock();
		}
		voxen::Log::info("Make new thread for task");

		// If we here, then no free threads, so construct new for the task
		// So, make worker, push task and run the worker
		ReportableWorker* new_worker = make_worker();
		future<T> result = task.get_future();
		task_pusher_function(&new_worker->state, task);

		new_worker->state.has_task = true;
		run_worker(new_worker);
		return result;
	}

private:
	vector<ReportableWorker*> threads;

	static std::chrono::milliseconds THREAD_LIVE_TIMEOUT;
	static size_t ETERNAL_THREAD_COUNT;
	static size_t START_THREAD_COUNT;
};

std::chrono::milliseconds FileIoThreadPool::THREAD_LIVE_TIMEOUT = 5000ms;
size_t FileIoThreadPool::ETERNAL_THREAD_COUNT = 2;
size_t FileIoThreadPool::START_THREAD_COUNT = FileIoThreadPool::ETERNAL_THREAD_COUNT;
}

static impl::FileIoThreadPool file_manager_threadpool;

namespace voxen {

string FileManager::profile_name = "default";

void FileManager::setProfileName(const string& profile_name)
{
	FileManager::profile_name = profile_name;
}

optional<dyn_array<std::byte>> FileManager::readUserFile(const path& relative_path) noexcept
{
	if (relative_path.empty())
		return std::nullopt;

	try {
		impl::RawBytesStorage storage;
		bool success = impl::readAbsPath(FileManager::userDataPath() / relative_path, storage);
		return success ? optional(dyn_array<std::byte>(storage.m_data, storage.m_size)) : std::nullopt;
	}
	catch (const std::bad_alloc &e) {
		voxen::Log::warn("Out of memory: {}", e.what());
		return std::nullopt;
	}
}

bool FileManager::writeUserFile(const path& relative_path, const void *data, size_t size) noexcept
{
	try {
		path filepath = FileManager::userDataPath() / relative_path;
		if (!FileManager::makeDirsForFile(filepath))
			return false;

		return impl::writeAbsPathFile(filepath, data, size);
	}
	catch (const std::bad_alloc &e) {
		voxen::Log::warn("Out of memory: {}", e.what());
		return false;
	}
}

optional<string> FileManager::readUserTextFile(const path& relative_path) noexcept
{
	if (relative_path.empty())
		return std::nullopt;

	try {
		impl::StringStorage storage;
		bool success = impl::readAbsPath(FileManager::userDataPath() / relative_path, storage);
		return success ? optional(storage.m_string) : std::nullopt;
	}
	catch (const std::bad_alloc &e) {
		voxen::Log::warn("Out of memory: {}", e.what());
		return std::nullopt;
	}
}

bool FileManager::writeUserTextFile(const path& relative_path, const string& text) noexcept
{
	try {
		path filepath = FileManager::userDataPath() / relative_path;
		if (!FileManager::makeDirsForFile(filepath))
			return false;

		return impl::writeAbsPathFile(filepath, text.data(), text.size());
	}
	catch (const std::bad_alloc &e) {
		voxen::Log::warn("Out of memory: {}", e.what());
		return false;
	}
}

optional<dyn_array<std::byte>> FileManager::readFile(const path& relative_path) noexcept
{
	if (relative_path.empty())
		return std::nullopt;

	try {
		impl::RawBytesStorage storage;
		bool success = impl::readAbsPath(FileManager::gameDataPath() / relative_path, storage);
		return success ? optional(dyn_array<std::byte>(storage.m_data, storage.m_size)) : std::nullopt;
	}
	catch (const std::bad_alloc &e) {
		voxen::Log::warn("Out of memory: {}", e.what());
		return std::nullopt;
	}
}

optional<string> FileManager::readTextFile(const path& relative_path) noexcept
{
	if (relative_path.empty())
		return std::nullopt;

	impl::StringStorage storage;
	bool success = impl::readAbsPath(FileManager::gameDataPath() / relative_path, storage);
	return success ? optional(storage.m_string) : std::nullopt;
}

bool FileManager::makeDirsForFile(const std::filesystem::path& relative_path) noexcept
{
	try {
		std::filesystem::create_directories(relative_path.parent_path());
	}
	catch (const std::bad_alloc &e) {
		voxen::Log::warn("Out of memory: {}", e.what());
		return false;
	}
	catch (const std::filesystem::filesystem_error &e) {
		voxen::Log::warn("Directory creation error: {}", e.what());
		return false;
	}

	return true;
}

path FileManager::userDataPath() noexcept
{
	if constexpr (BuildConfig::kIsDeployBuild) {
		return std::filesystem::path(sago::getDataHome()) / "voxen/";
	} else {
		return std::filesystem::current_path() / "data/user" / profile_name;
	}
}

path FileManager::gameDataPath() noexcept
{
	if constexpr (BuildConfig::kIsDeployBuild) {
		// TODO: implement me
		assert(false);
		return std::filesystem::path();
	} else {
		return std::filesystem::current_path() / "data/game";
	}
}

std::future<std::optional<extras::dyn_array<std::byte> > > FileManager::readFileAsync(const std::filesystem::path& relative_path) noexcept
{
	return file_manager_threadpool.enqueueFileRead(relative_path, FileManager::readFile);
}

std::future<std::optional<extras::dyn_array<std::byte> > > FileManager::readUserFileAsync(const std::filesystem::path& relative_path) noexcept
{
	return file_manager_threadpool.enqueueFileRead(relative_path, FileManager::readUserFile);
}

std::future<std::optional<std::string>> FileManager::readTextFileAsync(const std::filesystem::path& relative_path) noexcept
{
	return file_manager_threadpool.enqueueFileTextRead(relative_path, FileManager::readTextFile);
}

std::future<std::optional<std::string>> FileManager::readUserTextFileAsync(const std::filesystem::path& relative_path) noexcept
{
	return file_manager_threadpool.enqueueFileTextRead(relative_path, FileManager::readUserTextFile);
}

std::future<bool> FileManager::writeUserFileAsync(const std::filesystem::path& relative_path, const void* data, size_t size) noexcept
{
	return file_manager_threadpool.enqueueFileWrite(relative_path, std::bind(FileManager::writeUserFile, std::placeholders::_1, data, size));
}

std::future<bool> FileManager::writeUserTextFileAsync(const std::filesystem::path& relative_path, const std::string& text) noexcept
{
	return file_manager_threadpool.enqueueFileWrite(relative_path, std::bind(FileManager::writeUserTextFile, std::placeholders::_1, text));
}


}
