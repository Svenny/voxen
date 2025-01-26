#pragma once

#include <voxen/common/shared_object_pool.hpp>
#include <voxen/os/file.hpp>
#include <voxen/svc/service_base.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/visibility.hpp>

#include <extras/pimpl.hpp>

#include <cpp/result.hpp>

#include <span>
#include <system_error>

namespace voxen::svc
{

// Performs asynchronous (background) file I/O operations.
class VOXEN_API AsyncFileIoService final : public IService {
public:
	constexpr static UID SERVICE_UID = UID("91131570-ddfb7ba3-49b63d4d-04aaf4c8");
	constexpr static uint32_t FILE_HANDLE_POOL_HINT = 256;

	using Ptr = SharedPoolPtr<os::File, FILE_HANDLE_POOL_HINT>;
	using ReadResult = cpp::result<size_t, std::error_condition>;
	using WriteResult = cpp::result<void, std::error_condition>;

	struct Config {};

	AsyncFileIoService(ServiceLocator &svc, Config cfg);
	AsyncFileIoService(AsyncFileIoService &&) = delete;
	AsyncFileIoService(const AsyncFileIoService &) = delete;
	AsyncFileIoService &operator=(AsyncFileIoService &&) = delete;
	AsyncFileIoService &operator=(const AsyncFileIoService &) = delete;
	~AsyncFileIoService() override;

	UID serviceUid() const noexcept override { return SERVICE_UID; }

	// Put (register) a file handle into an internal pool for asynchronous I/O operations.
	// Handle ownership is captured in the returned pool pointer - it will be closed
	// automatically once the last live pointer instance is destroyed.
	//
	// File must have been opened with `FileFlagsBit::AsyncIo` flag, otherwise
	// asynchronous operations behavior is undefined (on Windows, that is).
	Ptr registerFile(os::File file);

	// Enqueue an asynchronous read (similar to `File::pread`)
	CoroFuture<ReadResult> asyncRead(Ptr file, std::span<std::byte> buffer, int64_t offset);
	// Enqueue an asynchronous write (similar to `File::pwrite`)
	CoroFuture<WriteResult> asyncWrite(Ptr file, std::span<const std::byte> buffer, int64_t offset);

private:
	extras::pimpl<detail::AsyncFileIoServiceImpl, 256, 8> m_impl;
};

} // namespace voxen::svc
