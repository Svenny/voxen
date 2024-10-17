#include <voxen/common/filemanager.hpp>

#include <voxen/os/stdlib.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <sago/platform_folders.h>

#include <atomic>
#include <cassert>
#include <fstream>
#include <functional>
#include <future>
#include <queue>
#include <system_error>
#include <thread>
#include <vector>

using extras::dyn_array;
using std::atomic_bool;
using std::filesystem::path;
using std::future;
using std::ifstream;
using std::ofstream;
using std::optional;
using std::packaged_task;
using std::string;
using std::thread;
using std::vector;

using namespace std::chrono_literals;

namespace voxen
{

namespace
{

void printErrno(const char* message, const path& filepath) noexcept
{
	auto ec = std::make_error_code(std::errc(errno));
	Log::error("{} `{}`, error: {}", message, filepath, ec);
}

struct RawBytesStorage {
	void setSize(size_t size) { m_data = extras::dyn_array<std::byte>(size); }
	void* data() noexcept { return m_data.data(); }

	extras::dyn_array<std::byte> m_data;
};

struct StringStorage {
	void setSize(size_t size) noexcept { m_string.resize(size, ' '); }
	void* data() noexcept { return m_string.data(); }

	string m_string;
};

template<typename T>
bool readAbsPath(const path& filepath, T& storageObject)
{
	FILE* pfile = fopen(filepath.string().c_str(), "rb");
	if (!pfile) {
		printErrno("Can't open file", filepath);
		return false;
	}
	// Don't forget to close when leaving the scope
	defer { fclose(pfile); };

	int64_t ssize = os::Stdlib::fileSize(pfile);
	if (ssize < 0) {
		printErrno("Can't get file size", filepath);
		return false;
	}

	if (ssize == 0) {
		// Empty file
		return true;
	}

	const size_t size = size_t(ssize);
	storageObject.setSize(size);

	size_t read = fread(storageObject.data(), 1, size, pfile);
	if (read != size) {
		printErrno("Can't read file", filepath);
		Log::error("{}/{} bytes read", read, size);
		return false;
	}

	return true;
}

bool writeAbsPathFile(const path& filepath, std::span<const std::byte> data)
{
	// To be "atomic", first write to a temporary
	// file, then rename it to the target name.
	path temp_path = filepath;
	temp_path += ".write-tmp";

	// TODO (Svenny): should we consider races (several threads opening the same file?)
	FILE* pfile = fopen(temp_path.string().c_str(), "wb");
	if (!pfile) {
		printErrno("Can't open temporary file", temp_path);
		return false;
	}

	defer {
		// Can be closed manually below
		if (pfile) {
			fclose(pfile);
		}
	};

	size_t written = fwrite(data.data(), 1, data.size(), pfile);
	if (written != data.size()) {
		printErrno("Can't write file", temp_path);
		Log::error("{}/{} bytes written", written, data.size());
		return false;
	}

	// Close before renaming
	fclose(pfile);
	pfile = nullptr;

	std::error_code ec;
	std::filesystem::rename(temp_path, filepath, ec);
	if (ec.value() != 0) {
		Log::error("Failed renaming `{}` to `{}`", temp_path, filepath);
		Log::error("Error code: {} ({})", ec, ec.default_error_condition().message());
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
		for (size_t i = 0; i < START_THREAD_COUNT; i++) {
			run_worker(make_worker());
		}
	}

	~FileIoThreadPool()
	{
		for (ReportableWorker* worker : threads) {
			std::unique_lock lock(worker->state.semaphore_mutex);

			worker->state.is_exit.store(true);
			worker->state.semaphore.notify_one();
		}

		for (ReportableWorker* worker : threads) {
			worker->worker.join();
		}

		while (threads.size() > 0) {
			delete threads.back();
			threads.pop_back();
		}
	}

	void enqueueTask(std::function<void()>&& task_function)
	{
		voxen::Log::info("enqueueTask");
		cleanup_finished_workers();

		packaged_task<void()> task(task_function);

		for (size_t i = 0; i < threads.size(); i++) {
			ReportableWorker* worker = threads[i];
			worker->state.state_mutex.lock();
			if (!worker->state.has_task) {
				worker->state.tasks_queue.push(std::move(task));
				worker->state.has_task = true;
				worker->state.state_mutex.unlock();

				worker->state.semaphore.notify_one();
				return;
			} else {
				worker->state.state_mutex.unlock();
			}
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

		std::unique_lock semaphore_lock(state->semaphore_mutex);

		while (!state->is_exit.load()) {
			state->state_mutex.lock();
			if (state->tasks_queue.size() >= 1) {
				packaged_task<void()> task = std::move(state->tasks_queue.front());
				state->tasks_queue.pop();
				state->state_mutex.unlock();

				task();

				state->state_mutex.lock();
				state->has_task = false;
			}
			state->state_mutex.unlock();

			if (is_eternal_thread) {
				state->semaphore.wait(semaphore_lock);
			} else {
				std::cv_status status = state->semaphore.wait_for(semaphore_lock, THREAD_LIVE_TIMEOUT);
				if (status == std::cv_status::timeout) {
					state->is_exit.store(true);
				}
			}
		}
	}

	ReportableWorker* make_worker()
	{
		ReportableWorker* new_worker = new ReportableWorker();
		new_worker->state.has_task = false;
		if (threads.size() < ETERNAL_THREAD_COUNT) {
			new_worker->state.live_forever = true;
		} else {
			new_worker->state.live_forever = false;
		}
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
		for (auto iter = threads.begin(); iter != threads.end();) {
			if ((*iter)->state.is_exit.load()) {
				delete *iter;
				iter = threads.erase(iter);
			} else {
				iter++;
			}
		}
	}

private:
	vector<ReportableWorker*> threads;

	constexpr static std::chrono::milliseconds THREAD_LIVE_TIMEOUT = 5000ms;
	constexpr static size_t ETERNAL_THREAD_COUNT = 2;
	constexpr static size_t START_THREAD_COUNT = FileIoThreadPool::ETERNAL_THREAD_COUNT;
};

} // anonymous namespace

static FileIoThreadPool file_manager_threadpool;

path FileManager::user_data_path;
path FileManager::game_data_path;

void FileManager::setProfileName(const std::string& path_to_binary, const std::string& profile_name)
{
	// Path to voxen binary is <install root>/bin/voxen, go two files above to get install root
	auto install_root = std::filesystem::path(path_to_binary).parent_path().parent_path();

	user_data_path = (install_root / "data" / "user" / profile_name).lexically_normal();
	game_data_path = (install_root / "data").lexically_normal();
}

optional<dyn_array<std::byte>> FileManager::readUserFile(const path& relative_path)
{
	if (relative_path.empty()) {
		return std::nullopt;
	}

	RawBytesStorage storage;
	bool success = readAbsPath(FileManager::userDataPath() / relative_path, storage);
	return success ? optional<dyn_array<std::byte>>(std::move(storage.m_data)) : std::nullopt;
}

bool FileManager::writeUserFile(const path& relative_path, std::span<const std::byte> data, bool create_directories)
{
	path filepath = FileManager::userDataPath() / relative_path;
	if (create_directories) {
		if (!FileManager::makeDirsForFile(filepath)) {
			return false;
		}
	}

	return writeAbsPathFile(filepath, data);
}

optional<string> FileManager::readUserTextFile(const path& relative_path)
{
	if (relative_path.empty()) {
		return std::nullopt;
	}

	StringStorage storage;
	bool success = readAbsPath(FileManager::userDataPath() / relative_path, storage);
	return success ? optional(std::move(storage.m_string)) : std::nullopt;
}

bool FileManager::writeUserTextFile(const path& relative_path, const string& text, bool create_directories)
{
	path filepath = FileManager::userDataPath() / relative_path;
	if (create_directories) {
		if (!FileManager::makeDirsForFile(filepath)) {
			return false;
		}
	}

	return writeAbsPathFile(filepath, std::as_bytes(std::span(text.data(), text.size())));
}

optional<dyn_array<std::byte>> FileManager::readFile(const path& relative_path)
{
	if (relative_path.empty()) {
		return std::nullopt;
	}

	RawBytesStorage storage;
	bool success = readAbsPath(FileManager::gameDataPath() / relative_path, storage);
	return success ? optional(dyn_array<std::byte>(std::move(storage.m_data))) : std::nullopt;
}

optional<string> FileManager::readTextFile(const path& relative_path)
{
	if (relative_path.empty()) {
		return std::nullopt;
	}

	StringStorage storage;
	bool success = readAbsPath(FileManager::gameDataPath() / relative_path, storage);
	return success ? optional(std::move(storage.m_string)) : std::nullopt;
}

bool FileManager::makeDirsForFile(const std::filesystem::path& relative_path)
{
	try {
		std::filesystem::create_directories(relative_path.parent_path());
	}
	catch (const std::filesystem::filesystem_error& e) {
		voxen::Log::error("Directory creation error: {}", e.what());
		return false;
	}

	return true;
}

std::future<std::optional<extras::dyn_array<std::byte>>> FileManager::readFileAsync(std::filesystem::path relative_path)
{
	std::shared_ptr<std::promise<optional<dyn_array<std::byte>>>> promise(
		new std::promise<optional<dyn_array<std::byte>>>);
	std::future<optional<dyn_array<std::byte>>> result = promise->get_future();

	std::function<void()> task_function(
		[filepath = std::move(relative_path), p = promise]() mutable { p->set_value(FileManager::readFile(filepath)); });
	file_manager_threadpool.enqueueTask(std::move(task_function));
	return result;
}

std::future<std::optional<extras::dyn_array<std::byte>>> FileManager::readUserFileAsync(
	std::filesystem::path relative_path)
{
	std::shared_ptr<std::promise<optional<dyn_array<std::byte>>>> promise(
		new std::promise<optional<dyn_array<std::byte>>>);
	std::future<optional<dyn_array<std::byte>>> result = promise->get_future();

	std::function<void()> task_function([filepath = std::move(relative_path), p = std::move(promise)]() mutable {
		p->set_value(FileManager::readUserFile(filepath));
	});
	file_manager_threadpool.enqueueTask(std::move(task_function));
	return result;
}

std::future<std::optional<std::string>> FileManager::readTextFileAsync(std::filesystem::path relative_path)
{
	std::shared_ptr<std::promise<optional<string>>> promise(new std::promise<optional<string>>);
	std::future<optional<string>> result = promise->get_future();

	std::function<void()> task_function([filepath = std::move(relative_path), p = std::move(promise)]() mutable {
		p->set_value(FileManager::readTextFile(filepath));
	});
	file_manager_threadpool.enqueueTask(std::move(task_function));
	return result;
}

std::future<std::optional<std::string>> FileManager::readUserTextFileAsync(std::filesystem::path relative_path)
{
	std::shared_ptr<std::promise<optional<string>>> promise(new std::promise<optional<string>>);
	std::future<optional<string>> result = promise->get_future();

	std::function<void()> task_function([filepath = std::move(relative_path), p = std::move(promise)]() mutable {
		p->set_value(FileManager::readUserTextFile(filepath));
	});
	file_manager_threadpool.enqueueTask(std::move(task_function));
	return result;
}

std::future<bool> FileManager::writeUserTextFileAsync(std::filesystem::path relative_path, std::string text,
	bool create_directories)
{
	std::shared_ptr<std::promise<bool>> promise(new std::promise<bool>);
	std::future<bool> result = promise->get_future();

	std::function<void()> task_function(
		[filepath = std::move(relative_path), p = std::move(promise), txt = std::move(text),
			create_directories]() mutable {
			p->set_value(FileManager::writeUserTextFile(filepath, std::move(txt), create_directories));
		});
	file_manager_threadpool.enqueueTask(std::move(task_function));
	return result;
}

} // namespace voxen
