#include <voxen/os/file.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/hash.hpp>
#include <voxen/util/log.hpp>

#ifndef _WIN32
	#include <fcntl.h>
	#include <sys/file.h>
	#include <sys/stat.h>
#else
	#define NOMINMAX
	#include <Windows.h>
#endif

#include <chrono>
#include <cinttypes>

namespace voxen::os
{

#ifdef _WIN32
static_assert(std::is_same_v<HANDLE, void *>, "HANDLE is not equal to void *");

const File::NativeHandle File::INVALID_HANDLE = reinterpret_cast<void *>(~uintptr_t(0));

// Global storage to help generate random temporary file names, see `doOpen()`
static std::atomic_uint64_t g_temp_file_randomizer = 0;
#endif

namespace
{

bool sanitizeOpenFlags(FileFlags &flags) noexcept
{
	constexpr FileFlags LOCK_CONFLICT { FileFlagsBit::LockShared, FileFlagsBit::LockExclusive };
	constexpr FileFlags HINT_CONFLICT { FileFlagsBit::HintSequentialAccess, FileFlagsBit::HintRandomAccess };

	if (flags.test_all(LOCK_CONFLICT)) [[unlikely]] {
		return false;
	}

	if (flags.test_all(HINT_CONFLICT)) [[unlikely]] {
		return false;
	}

	if (flags.test(FileFlagsBit::CreateSubdirs)) {
		flags.set(FileFlagsBit::Create);
	}

	if (flags.test(FileFlagsBit::TempFile)) {
		flags.set(FileFlagsBit::Create);
		flags.set(FileFlagsBit::Write);
	}

	return true;
}

void doClose(File::NativeHandle handle)
{
#ifndef _WIN32
	// BSD file locks are automatically released when handle is closed
	int err = close(handle);

	if (err != 0) [[unlikely]] {
		// This is not expected at all, however we can't do anything other than just log it
		Log::warn("close() of a file descriptor failed, error {} ({})", err, std::system_category().message(err));
	}
#else
	if (!CloseHandle(handle)) [[unlikely]] {
		// This is not expected at all, however we can't do anything other than just log it
		int err = static_cast<int>(GetLastError());
		Log::warn("CloseHandle() of a file descriptor failed, error {} ({})", err, std::system_category().message(err));
	}
#endif
}

cpp::result<File::NativeHandle, std::error_code> doOpen(const std::filesystem::path &path, FileFlags flags)
{
#ifndef _WIN32
	// 644 (rw-r--r--)
	constexpr mode_t CREATE_MODE = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	// Not that we're going to call `exec` anywhere,
	// this flag just should always be set by default
	int open_flags = O_CLOEXEC | O_LARGEFILE;

	if (flags.test_all({ FileFlagsBit::Read, FileFlagsBit::Write })) {
		open_flags |= O_RDWR;
	} else if (flags.test(FileFlagsBit::Read)) {
		open_flags |= O_RDONLY;
	} else if (flags.test(FileFlagsBit::Write)) {
		open_flags |= O_WRONLY;
	}

	if (flags.test(FileFlagsBit::TempFile)) {
		open_flags |= O_TMPFILE;
	} else {
		// Experimentally found that combination of `O_TMPFILE` with these gives EINVAL
		if (flags.test(FileFlagsBit::Create)) {
			open_flags |= O_CREAT;
		}

		if (flags.test(FileFlagsBit::Truncate)) {
			open_flags |= O_TRUNC;
		}
	}

	int fd = open(path.c_str(), open_flags, CREATE_MODE);
	if (fd == -1) [[unlikely]] {
		std::error_code ec(errno, std::system_category());
		return cpp::failure(ec);
	}

	int flock_result = 0;

	// Take BSD advisory lock if requested
	if (flags.test(FileFlagsBit::LockShared)) {
		flock_result = flock(fd, LOCK_SH | LOCK_NB);
	} else if (flags.test(FileFlagsBit::LockExclusive)) {
		flock_result = flock(fd, LOCK_EX | LOCK_NB);
	}

	if (flock_result == -1) [[unlikely]] {
		std::error_code ec(errno, std::system_category());
		doClose(fd);
		return cpp::failure(ec);
	}

	int fadvise_result = 0;

	if (flags.test(FileFlagsBit::HintRandomAccess)) {
		fadvise_result = posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM);
	} else if (flags.test(FileFlagsBit::HintSequentialAccess)) {
		fadvise_result = posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
	}

	if (fadvise_result != 0) [[unlikely]] {
		// This is not fatal, we can proceed without fadvise
		std::error_code ec(fadvise_result, std::system_category());
		Log::warn("'posix_fadvise' failed while opening '{}': {} ({})", path, ec, ec.message());
	}

	return fd;
#else
	DWORD access = 0;
	DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	DWORD creation_mode = OPEN_EXISTING;
	DWORD flags_attrs = FILE_ATTRIBUTE_NORMAL;

	if (flags.test(FileFlagsBit::Read)) {
		access |= GENERIC_READ;
	}
	if (flags.test(FileFlagsBit::Write)) {
		access |= GENERIC_WRITE | DELETE;
	}

	if (flags.test(FileFlagsBit::LockShared)) {
		share_mode = FILE_SHARE_READ;
	} else if (flags.test(FileFlagsBit::LockExclusive)) {
		share_mode = 0;
	}

	if (flags.test(FileFlagsBit::Create)) {
		creation_mode = flags.test(FileFlagsBit::Truncate) ? CREATE_ALWAYS : OPEN_ALWAYS;
	} else if (flags.test(FileFlagsBit::Truncate)) {
		creation_mode = TRUNCATE_EXISTING;
	}

	if (flags.test(FileFlagsBit::AsyncIo)) {
		flags_attrs |= FILE_FLAG_OVERLAPPED;
	}
	if (flags.test(FileFlagsBit::HintRandomAccess)) {
		flags_attrs |= FILE_FLAG_RANDOM_ACCESS;
	}
	if (flags.test(FileFlagsBit::HintSequentialAccess)) {
		flags_attrs |= FILE_FLAG_SEQUENTIAL_SCAN;
	}

	const std::filesystem::path *argument_path = &path;

	std::filesystem::path temp_file_path;

	if (flags.test(FileFlagsBit::TempFile)) {
		flags_attrs |= FILE_FLAG_DELETE_ON_CLOSE;

		// Take current time, hash it, then fetch_add to `g_temp_file_randomizer`.
		// This should give different names even if some timestamps do collide.
		auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();

		uint64_t hash = Hash::xxh64Fixed(static_cast<uint64_t>(timestamp));
		// Replicate fetch_add result in our local variable
		hash += g_temp_file_randomizer.fetch_add(hash, std::memory_order_relaxed);

		char name_buf[32];
		snprintf(name_buf, std::size(name_buf), "%" PRIx64 ".tmp", hash);

		temp_file_path = path / std::string_view(name_buf);
		argument_path = &temp_file_path;
	}

	// We accept the (practically zero?) probability of temp file name collision and do not retry
	HANDLE handle = CreateFileW(argument_path->c_str(), access, share_mode, nullptr, creation_mode, flags_attrs,
		nullptr);
	if (handle == INVALID_HANDLE_VALUE) [[unlikely]] {
		std::error_code ec(static_cast<int>(GetLastError()), std::system_category());
		return cpp::failure(ec);
	}

	return handle;
#endif
}

#ifndef _WIN32
File::Stat translateStat(const struct stat64 &st)
{
	return {
		.size = st.st_size,
		.ctime = std::filesystem::file_time_type::min() + std::chrono::seconds(st.st_ctim.tv_sec)
			+ std::chrono::nanoseconds(st.st_ctim.tv_nsec),
		.mtime = std::filesystem::file_time_type::min() + std::chrono::seconds(st.st_mtim.tv_sec)
			+ std::chrono::nanoseconds(st.st_mtim.tv_nsec),
	};
}
#else
// `FILETIME` struct specifies 100 ns intervals (1/10^7 seconds)
using FileTimeDuration = std::chrono::duration<int64_t, std::ratio<1LL, 10'000'000LL> >;

FileTimeDuration getFileTimeDuration(FILETIME ft) noexcept
{
	int64_t value = static_cast<int64_t>(ft.dwHighDateTime) << 32;
	value |= static_cast<int64_t>(ft.dwLowDateTime);
	return FileTimeDuration { value };
}
#endif

} // namespace

File::File(File &&other) noexcept : m_handle(std::exchange(other.m_handle, INVALID_HANDLE)) {}

File &File::operator=(File &&other) noexcept
{
	std::swap(m_handle, other.m_handle);
	return *this;
}

File::~File()
{
	if (m_handle != INVALID_HANDLE) {
		doClose(m_handle);
	}
}

File::File(NativeHandle native) noexcept : m_handle(native) {}

File::Stat File::stat()
{
#ifndef _WIN32
	struct stat64 st {};

	int res = fstat64(m_handle, &st);
	if (res < 0) [[unlikely]] {
		throw Exception::fromErrorCode({ errno, std::system_category() }, "'fstat' failed");
	}

	return translateStat(st);
#else
	BY_HANDLE_FILE_INFORMATION info {};

	if (!GetFileInformationByHandle(m_handle, &info)) [[unlikely]] {
		std::error_code ec(static_cast<int>(GetLastError()), std::system_category());
		throw Exception::fromErrorCode(ec, "'GetFileInformationByHandle' failed");
	}

	return {
		.size = (static_cast<int64_t>(info.nFileSizeHigh) << 32) | static_cast<int64_t>(info.nFileSizeLow),
		.ctime = std::filesystem::file_time_type::min() + getFileTimeDuration(info.ftCreationTime),
		.mtime = std::filesystem::file_time_type::min() + getFileTimeDuration(info.ftLastWriteTime),
	};
#endif
}

void File::materializeTempFile(const std::filesystem::path &path)
{
#ifndef _WIN32
	// Unfortunately `AT_EMPTY_PATH` is broken and gives ENOENT
	char oldpath[32];
	snprintf(oldpath, std::size(oldpath), "/proc/self/fd/%d", m_handle);

	int res = linkat(AT_FDCWD, oldpath, AT_FDCWD, path.c_str(), AT_SYMLINK_FOLLOW);
	if (res < 0) [[unlikely]] {
		throw Exception::fromErrorCode({ errno, std::system_category() }, "'linkat' failed");
	}
#else
	static_assert(std::is_same_v<wchar_t, std::filesystem::path::value_type>, "Windows path value type must be wchar_t");

	// Don't need to count in null terminator - `FILE_RENAME_INFO` already has 1 byte in `FileName` array
	const DWORD path_size = static_cast<DWORD>(sizeof(wchar_t) * path.native().length());
	const DWORD struct_size = sizeof(FILE_RENAME_INFO) + path_size;
	auto bytes = std::make_unique<std::byte[]>(struct_size);

	FILE_RENAME_INFO *fri = new (bytes.get()) FILE_RENAME_INFO {};
	fri->Flags = FILE_RENAME_FLAG_REPLACE_IF_EXISTS | FILE_RENAME_FLAG_POSIX_SEMANTICS;
	fri->FileNameLength = path_size; // Yes, length in bytes, without null termination
	wcscpy(fri->FileName, path.c_str());

	if (!SetFileInformationByHandle(m_handle, FileRenameInfoEx, fri, struct_size)) [[unlikely]] {
		int ec = static_cast<int>(GetLastError());
		throw Exception::fromErrorCode({ ec, std::system_category() }, "'SetFileInformationByHandle' failed");
	}

	// Disable delete-on-close. It seems that documentation for new win10
	// file API features exists only as random posts on various websites:
	// https://community.osr.com/t/unset-file-flag-delete-on-close/57315/7
	FILE_DISPOSITION_INFO_EX fdi { .Flags = FILE_DISPOSITION_FLAG_DO_NOT_DELETE | FILE_DISPOSITION_FLAG_ON_CLOSE };
	if (!SetFileInformationByHandle(m_handle, FileDispositionInfoEx, &fdi, sizeof(fdi))) [[unlikely]] {
		int ec = static_cast<int>(GetLastError());
		throw Exception::fromErrorCode({ ec, std::system_category() }, "'SetFileInformationByHandle' failed");
	}
#endif
}

size_t File::read(std::span<std::byte> buffer)
{
#ifndef _WIN32
	size_t read_bytes = 0;

	while (buffer.size() > 0) {
		ssize_t read_result = ::read(m_handle, buffer.data(), buffer.size());
		if (read_result < 0) [[unlikely]] {
			if (errno == EINTR) {
				// Ignore signal interruptions
				continue;
			}

			throw Exception::fromErrorCode({ errno, std::system_category() }, "'read' failed");
		}

		if (read_result == 0) {
			// EOF, stop here
			return read_bytes;
		}

		// Incomplete transfer, try again
		size_t transferred = static_cast<size_t>(read_result);
		read_bytes += transferred;
		buffer = buffer.subspan(transferred);
	}

	return read_bytes;
#else
	size_t read_bytes = 0;

	while (buffer.size() > 0) {
		DWORD read_result = 0;
		DWORD read_size = static_cast<DWORD>(std::min<size_t>(buffer.size(), UINT32_MAX));
		if (!ReadFile(m_handle, buffer.data(), read_size, &read_result, nullptr)) [[unlikely]] {
			int ec = static_cast<int>(GetLastError());
			throw Exception::fromErrorCode({ ec, std::system_category() }, "'ReadFile' failed");
		}

		if (read_result == 0) {
			// EOF, stop here
			return read_bytes;
		}

		// Incomplete transfer, try again (is it even possible on Windows? MSDN says nothing...)
		// At least it can happen if `buffer` is larger than `UINT32_MAX` and we split reads.
		read_bytes += read_result;
		buffer = buffer.subspan(read_result);
	}

	return read_bytes;
#endif
}

size_t File::pread(std::span<std::byte> buffer, int64_t offset)
{
#ifndef _WIN32
	size_t read_bytes = 0;

	while (buffer.size() > 0) {
		ssize_t read_result = pread64(m_handle, buffer.data(), buffer.size(), offset);
		if (read_result < 0) [[unlikely]] {
			if (errno == EINTR) {
				// Ignore signal interruptions
				continue;
			}

			throw Exception::fromErrorCode({ errno, std::system_category() }, "'pread' failed");
		}

		if (read_result == 0) {
			// EOF, stop here
			return read_bytes;
		}

		// Incomplete transfer, try again
		size_t transferred = static_cast<size_t>(read_result);
		read_bytes += transferred;
		buffer = buffer.subspan(transferred);
		offset += read_result;
	}

	return read_bytes;
#else
	size_t read_bytes = 0;

	while (buffer.size() > 0) {
		DWORD read_result = 0;
		DWORD read_size = static_cast<DWORD>(std::min<size_t>(buffer.size(), UINT32_MAX));

		OVERLAPPED ovl {};
		ovl.Offset = static_cast<DWORD>(offset);
		ovl.OffsetHigh = static_cast<DWORD>(offset >> 32);

		BOOL ok = ReadFile(m_handle, buffer.data(), read_size, &read_result, &ovl);
		DWORD gle = GetLastError();

		if (!ok && gle != ERROR_HANDLE_EOF) [[unlikely]] {
			throw Exception::fromErrorCode({ static_cast<int>(gle), std::system_category() }, "'ReadFile' failed");
		}

		if (read_result == 0) {
			// EOF, stop here
			return read_bytes;
		}

		// Incomplete transfer, try again (is it even possible on Windows? MSDN says nothing...)
		// At least it can happen if `buffer` is larger than `UINT32_MAX` and we split reads.
		read_bytes += read_result;
		buffer = buffer.subspan(read_result);
		offset += static_cast<int64_t>(read_result);
	}

	return read_bytes;
#endif
}

void File::write(std::span<const std::byte> buffer)
{
#ifndef _WIN32
	while (buffer.size() > 0) {
		ssize_t write_result = ::write(m_handle, buffer.data(), buffer.size());
		if (write_result < 0) [[unlikely]] {
			if (errno == EINTR) {
				// Ignore signal interruptions
				continue;
			}

			throw Exception::fromErrorCode({ errno, std::system_category() }, "'write' failed");
		}

		// Incomplete transfer, try again.
		// XXX: can `write()` return zero? Then we could get stuck here.
		buffer = buffer.subspan(static_cast<size_t>(write_result));
	}
#else
	while (buffer.size() > 0) {
		DWORD write_result = 0;
		DWORD write_size = static_cast<DWORD>(std::min<size_t>(buffer.size(), UINT32_MAX));
		if (!WriteFile(m_handle, buffer.data(), write_size, &write_result, nullptr)) [[unlikely]] {
			int ec = static_cast<int>(GetLastError());
			throw Exception::fromErrorCode({ ec, std::system_category() }, "'WriteFile' failed");
		}

		// Incomplete transfer, try again (is it even possible on Windows? MSDN says nothing...)
		// At least it can happen if `buffer` is larger than `UINT32_MAX` and we split writes.
		// XXX: can `WriteFile()` return zero? Then we could get stuck here.
		buffer = buffer.subspan(write_result);
	}
#endif
}

void File::pwrite(std::span<const std::byte> buffer, int64_t offset)
{
#ifndef _WIN32
	while (buffer.size() > 0) {
		ssize_t write_result = pwrite64(m_handle, buffer.data(), buffer.size(), offset);
		if (write_result < 0) [[unlikely]] {
			if (errno == EINTR) {
				// Ignore signal interruptions
				continue;
			}

			throw Exception::fromErrorCode({ errno, std::system_category() }, "'pwrite' failed");
		}

		// Incomplete transfer, try again.
		// XXX: can `pwrite()` return zero? Then we could get stuck here.
		buffer = buffer.subspan(static_cast<size_t>(write_result));
		offset += write_result;
	}
#else
	while (buffer.size() > 0) {
		DWORD write_result = 0;
		DWORD write_size = static_cast<DWORD>(std::min<size_t>(buffer.size(), UINT32_MAX));

		OVERLAPPED ovl {};
		ovl.Offset = static_cast<DWORD>(offset);
		ovl.OffsetHigh = static_cast<DWORD>(offset >> 32);

		if (!WriteFile(m_handle, buffer.data(), write_size, &write_result, &ovl)) [[unlikely]] {
			int ec = static_cast<int>(GetLastError());
			throw Exception::fromErrorCode({ ec, std::system_category() }, "'WriteFile' failed");
		}

		// Incomplete transfer, try again (is it even possible on Windows? MSDN says nothing...)
		// At least it can happen if `buffer` is larger than `UINT32_MAX` and we split writes.
		// XXX: can `WriteFile()` return zero? Then we could get stuck here.
		buffer = buffer.subspan(write_result);
		offset += static_cast<int64_t>(write_result);
	}
#endif
}

File File::open(const std::filesystem::path &path, FileFlags flags)
{
	auto open_result = tryOpen(path, flags);
	if (open_result.has_error()) {
		throw Exception::fromError(open_result.error(), "'File::open' failed");
	}

	return *std::move(open_result);
}

cpp::result<File, std::error_condition> File::tryOpen(const std::filesystem::path &path, FileFlags flags)
{
	if (!sanitizeOpenFlags(flags)) [[unlikely]] {
		return cpp::failure(std::errc::invalid_argument);
	}

	if (flags.test(FileFlagsBit::CreateSubdirs)) {
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);

		if (ec) [[unlikely]] {
			Log::error("Subdirectories create failed for '{}', error {} ({})", path, ec, ec.message());
			return cpp::failure(ec.default_error_condition());
		}
	}

	auto open_result = doOpen(path, flags);
	if (open_result.has_value()) [[likely]] {
		// Transfer handle ownership to the RAII object
		return File(open_result.value());
	}

	// Don't log anything, failures here can be expected (e.g. trying to
	// open a non-existing file when it's not critical for operation).
	// Anyway the caller should react on this, it will likely log more informative than us.
	return cpp::failure(open_result.error().default_error_condition());
}

cpp::result<File::Stat, std::error_condition> File::stat(const std::filesystem::path &path)
{
#ifndef _WIN32
	struct stat64 st {};

	int res = stat64(path.c_str(), &st);
	if (res < 0) [[unlikely]] {
		return cpp::failure(std::system_category().default_error_condition(errno));
	}

	return translateStat(st);
#else
	// Looks like there is nothing like `stat()` on windows...
	auto open_result = tryOpen(path, FileFlags {});
	if (open_result.has_error()) {
		return cpp::failure(open_result.error());
	}

	return open_result->stat();
#endif
}

} // namespace voxen::os
