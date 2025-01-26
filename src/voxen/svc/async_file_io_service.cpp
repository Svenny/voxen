#include <voxen/svc/async_file_io_service.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/debug/thread_name.hpp>
#include <voxen/os/futex.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/svc/task_coro.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/futex_work_counter.hpp>

#include "async_counter_tracker.hpp"

#include <extras/defer.hpp>

#include <queue>
#include <thread>
#include <variant>

namespace voxen::svc
{

namespace
{

struct FileReadCommand {
	std::span<std::byte> buffer;
	int64_t offset;
	std::shared_ptr<AsyncFileIoService::ReadResult> result_ptr;
};

struct FileWriteCommand {
	std::span<const std::byte> buffer;
	int64_t offset;
	std::shared_ptr<AsyncFileIoService::WriteResult> result_ptr;
};

} // namespace

class detail::AsyncFileIoServiceImpl {
public:
	struct IoQueueItem {
		uint64_t async_counter;
		AsyncFileIoService::Ptr file_ptr;
		std::variant<FileReadCommand, FileWriteCommand> command;
	};

	AsyncFileIoServiceImpl(ServiceLocator &svc, AsyncFileIoService::Config cfg)
		: m_cfg(std::move(cfg)), m_counter_tracker(svc.requestService<AsyncCounterTracker>())
	{
		svc.requestService<PipeMemoryAllocator>();
		m_io_thread = std::thread(ioThreadProc, std::ref(*this));
	}

	~AsyncFileIoServiceImpl()
	{
		m_io_work_counter.requestStop();
		m_io_thread.join();
	}

	AsyncFileIoService::Ptr registerFile(os::File file) { return m_file_handle_pool.allocate(std::move(file)); }

	uint64_t allocateAsyncCounter() { return m_counter_tracker.allocateCounter(); }

	void pushIoCommand(IoQueueItem item)
	{
		{
			std::lock_guard lock(m_io_queue_lock);
			m_io_queue.emplace(std::move(item));
		}

		m_io_work_counter.addWork(1);
	}

private:
	AsyncFileIoService::Config m_cfg;
	AsyncCounterTracker &m_counter_tracker;

	SharedObjectPool<os::File, AsyncFileIoService::FILE_HANDLE_POOL_HINT> m_file_handle_pool;

	std::thread m_io_thread;
	FutexWorkCounter m_io_work_counter;
	os::FutexLock m_io_queue_lock;
	std::queue<IoQueueItem> m_io_queue;

	// This thread processes queued I/O commands and performs them
	//	in a blocking fashion (with `File::pread()/pwrite()` calls).
	//
	// TODO: this is not the right way to implement asynchronous I/O.
	// We should use designated platform-specific APIs:
	// io_uring on Linux and OVERLAPPED/threadpool/IOCP/ on Windows.
	// Now it's done like that only to bring up the engine interface.
	//
	// NOTE: see commented out piece about `AsyncIo` flag in
	// windows implementation of `File`. It must be uncommented
	// once we rewrite this part to the proper asynchronous calls.
	static void ioThreadProc(AsyncFileIoServiceImpl &me)
	{
		debug::setThreadName("FileIoThread");

		uint32_t work_count = 0;
		bool stop_requested = false;

		while (!stop_requested) {
			if (work_count == 0) {
				std::tie(work_count, stop_requested) = me.m_io_work_counter.wait();
			}

			uint32_t work_remaining = work_count;

			while (work_remaining > 0) {
				IoQueueItem item;

				{
					std::lock_guard lock(me.m_io_queue_lock);
					item = std::move(me.m_io_queue.front());
					me.m_io_queue.pop();
				}

				work_remaining--;

				if (const FileReadCommand *read_cmd = std::get_if<FileReadCommand>(&item.command); read_cmd) {
					try {
						*read_cmd->result_ptr = item.file_ptr->pread(read_cmd->buffer, read_cmd->offset);
					}
					catch (Exception &ex) {
						*read_cmd->result_ptr = cpp::failure(ex.error());
					}
					catch (...) {
						*read_cmd->result_ptr = cpp::failure(VoxenErrc::UnknownError);
					}
				}

				if (const FileWriteCommand *write_cmd = std::get_if<FileWriteCommand>(&item.command); write_cmd) {
					try {
						item.file_ptr->pwrite(write_cmd->buffer, write_cmd->offset);
					}
					catch (Exception &ex) {
						*write_cmd->result_ptr = cpp::failure(ex.error());
					}
					catch (...) {
						*write_cmd->result_ptr = cpp::failure(VoxenErrc::UnknownError);
					}
				}

				me.m_counter_tracker.completeCounter(item.async_counter);
			}

			std::tie(work_count, stop_requested) = me.m_io_work_counter.removeWork(work_count);
		}
	}
};

AsyncFileIoService::AsyncFileIoService(ServiceLocator &svc, Config cfg) : m_impl(svc, cfg) {}

AsyncFileIoService::~AsyncFileIoService() = default;

auto AsyncFileIoService::registerFile(os::File file) -> Ptr
{
	return m_impl->registerFile(std::move(file));
}

auto AsyncFileIoService::asyncRead(Ptr file, std::span<std::byte> buffer, int64_t offset) -> CoroFuture<ReadResult>
{
	uint64_t counter = m_impl->allocateAsyncCounter();
	auto result_ptr = std::allocate_shared<ReadResult>(TPipeMemoryAllocator<ReadResult>());

	m_impl->pushIoCommand({
		.async_counter = counter,
		.file_ptr = std::move(file),
		.command = FileReadCommand { buffer, offset, result_ptr },
	});

	return { counter, std::move(result_ptr) };
}

auto AsyncFileIoService::asyncWrite(Ptr file, std::span<const std::byte> buffer, int64_t offset)
	-> CoroFuture<WriteResult>
{
	uint64_t counter = m_impl->allocateAsyncCounter();
	auto result_ptr = std::allocate_shared<WriteResult>(TPipeMemoryAllocator<WriteResult>());

	m_impl->pushIoCommand({
		.async_counter = counter,
		.file_ptr = std::move(file),
		.command = FileWriteCommand { buffer, offset, result_ptr },
	});

	return { counter, std::move(result_ptr) };
}

} // namespace voxen::svc
