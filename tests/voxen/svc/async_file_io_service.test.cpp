#include <voxen/svc/async_file_io_service.hpp>

#include <voxen/svc/engine.hpp>
#include <voxen/svc/task_builder.hpp>
#include <voxen/svc/task_coro.hpp>
#include <voxen/svc/task_service.hpp>

#include <extras/defer.hpp>
#include <extras/dyn_array.hpp>

#include "../../voxen_test_common.hpp"

#include <fmt/format.h>

#include <vector>

namespace voxen::svc
{

TEST_CASE("'AsyncFileIoService' test case 1", "[voxen::svc::async_file_io_service]")
{
	std::filesystem::path tmp_path = std::filesystem::temp_directory_path() / "test-voxen-file-aio-case1";
	INFO("Temporary directory: " << tmp_path);

	REQUIRE_NOTHROW(std::filesystem::create_directory(tmp_path));
	// Clean up any mess after test sections
	defer { std::filesystem::remove_all(tmp_path); };

	auto engine = Engine::createForTestSuite();
	AsyncFileIoService &aio_svc = engine->serviceLocator().requestService<AsyncFileIoService>();
	TaskService &task_svc = engine->serviceLocator().requestService<TaskService>();

	constexpr size_t N = 10;
	std::vector<uint64_t> task_counters(N);
	// TODO: we should use something like "error message queue" to make it clear
	// where errors happened. This should be a common utility for many async tests.
	std::atomic_size_t errors = 0;

	// Write some files
	for (size_t i = 0; i < N; i++) {
		auto write_coro = [](std::atomic_size_t &errs, AsyncFileIoService &srv, std::filesystem::path path) -> CoroTask {
			os::FileFlags flags { os::FileFlagsBit::AsyncIo, os::FileFlagsBit::Write, os::FileFlagsBit::CreateSubdirs,
				os::FileFlagsBit::LockExclusive };

			auto maybe_file = os::File::tryOpen(path, flags);
			if (!maybe_file.has_value()) {
				errs.fetch_add(1);
				co_return;
			}

			// Simply write file path into the file
			auto buffer = std::as_bytes(std::span(path.native()));

			AsyncFileIoService::Ptr file = srv.registerFile(*std::move(maybe_file));
			AsyncFileIoService::WriteResult result = co_await srv.asyncWrite(std::move(file), buffer, 0);

			if (result.has_error()) {
				errs.fetch_add(1);
			}
		};

		TaskBuilder bld(task_svc);
		bld.enqueueTask(write_coro(errors, aio_svc, tmp_path / fmt::format("file{}.txt", i + 1)));
		task_counters[i] = bld.getLastTaskCounter();
	}

	// Read them back and verify
	for (size_t i = 0; i < N; i++) {
		auto read_coro = [](std::atomic_size_t &errs, AsyncFileIoService &srv, std::filesystem::path path) -> CoroTask {
			os::FileFlags flags { os::FileFlagsBit::AsyncIo, os::FileFlagsBit::Read, os::FileFlagsBit::LockShared };

			auto maybe_file = os::File::tryOpen(path, flags);
			if (!maybe_file.has_value()) {
				errs.fetch_add(1);
				co_return;
			}

			extras::dyn_array<std::filesystem::path::value_type> buffer(path.native().size());

			AsyncFileIoService::Ptr file = srv.registerFile(*std::move(maybe_file));
			AsyncFileIoService::ReadResult result = co_await srv.asyncRead(std::move(file), buffer.as_writable_bytes(), 0);

			if (result.has_error() || *result != buffer.size_bytes()) {
				errs.fetch_add(1);
				co_return;
			}

			if (memcmp(buffer.data(), path.native().c_str(), buffer.size_bytes()) != 0) {
				errs.fetch_add(1);
			}
		};

		TaskBuilder bld(task_svc);
		bld.addWait(task_counters[i]);
		bld.enqueueTask(read_coro(errors, aio_svc, tmp_path / fmt::format("file{}.txt", i + 1)));
		task_counters[i] = bld.getLastTaskCounter();
	}

	TaskBuilder bld(task_svc);
	bld.addWait(task_counters);
	bld.enqueueSyncPoint().wait();

	CHECK(errors.load() == 0);
}

} // namespace voxen::svc
