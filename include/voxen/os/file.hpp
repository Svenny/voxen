#pragma once

#include <voxen/visibility.hpp>

#include <extras/enum_utils.hpp>

#include <cpp/result.hpp>

#include <cstdint>
#include <filesystem>
#include <span>

namespace voxen::os
{

// These flags control `File::open()` behavior
enum class FileFlagsBit : uint32_t {
	None = 0,
	// Allow reading from the opened file
	Read = 1u << 1,
	// Allow writing to the opened file.
	// On Windows, also allows `DELETE` operations.
	Write = 1u << 2,
	// On Windows, limits file sharing mode to `FILE_SHARE_READ`.
	// On Linux, takes shared advisory file lock with `flock`.
	// If locking fails then open fails as well, it does not wait.
	//
	// Lock is automatically released when the file handle is closed.
	// It's an error to combine this with `LockExclusive`.
	LockShared = 1u << 3,
	// On Windows, limits file sharing mode to `FILE_SHARE_NONE`.
	// On Linux, takes exclusive advisory file lock with `flock`.
	// If locking fails then open fails as well, it does not wait.
	//
	// Lock is automatically released when the file handle is closed.
	// It's an error to combine this with `LockShared`.
	LockExclusive = 1u << 4,
	// Create a new file if it does not exist or open the existing file.
	// On Linux, the newly created file gets 644 (rw-r--r--) permissions.
	// Permissions of already existing files do not change.
	Create = 1u << 5,
	// Create (recursively) missing directories along the file path.
	// Open fails if any directory could not be created.
	// Implicitly adds `Create` flag.
	CreateSubdirs = 1u << 6,
	// If the opened file already exists, it will be
	// truncated to zero length. Otherwise does nothing.
	Truncate = 1u << 7,
	// On Windows, open the file with `FILE_ATTRIBUTE_TEMPORARY` and set
	// `FILE_DISPOSITION_INFO.DeleteFile = TRUE`. On Linux, use `O_TMPFILE` flag.
	// This file will automatically delete after the last handle to it closes.
	//
	// When this flag is provided, `path` must refer to the containing directory
	// instead of file. File name will be either empty or assigned randomly.
	// Implicilty adds `Create` and `Write` flags.
	TempFile = 1u << 8,
	// On Windows, opens the file with `FILE_FLAG_OVERLAPPED`, allowing
	// non-blocking read/write operations. Ignored on other platforms.
	AsyncIo = 1u << 9,
	// Hint the prefetcher (readahead) that this file will be accessed
	// mostly randomly (what is "randomly enough" is not specified).
	// This might e.g. decrease or disable the prefetch window.
	// It's an error to combine this with `HintSequentialAccess`.
	HintRandomAccess = 1u << 10,
	// Hint the prefetcher (readahead) that this file will be accessed
	// sequentially from beginning to end (but not in reverse).
	// This might e.g. increase the prefetch window size.
	// It's an error to combine this with `HintRandomAccess`.
	HintSequentialAccess = 1u << 11,
};

// Bitmask of `FileFlagsBit`
using FileFlags = extras::enum_flags<FileFlagsBit>;

// A handle to the OS file descriptor with `std::unique_ptr`-like semantics.
//
// This class offers the lowest abstraction layer and synchronous (blocking)
// operations only. For higher-level operations like asynchronous I/O
// or "VFS/mount points" you should use higher-level services.
//
// It supports file locking which is mandatory on Windows but advisory on Linux.
// This means that on Linux some non-cooperating code is free to ignore locks
// and corrupt your files. However, locks are more of a guard rail anyway,
// not a robust protection measure. Also, as long as all engine and tooling code
// performs file operations through this wrapper (and does not forget to use flags),
// locks will work as intended on both platforms, ensuring that e.g. a build tool
// will not corrupt archive files currently opened by a running engine instance.
//
// All operations on invalid (default-constructed) file handles
// are invalid and will throw errors from underlying syscalls.
class VOXEN_API File {
public:
#ifndef _WIN32
	using NativeHandle = int;
	constexpr static NativeHandle INVALID_HANDLE = -1;
#else
	using NativeHandle = void *;
	// It should have been defined inline like this:
	//    constexpr static NativeHandle INVALID_HANDLE = NativeHandle(~uintptr_t(0));
	// but unfortunately C++ standard is broken, it forbids constexpr pointer casts.
	const static NativeHandle INVALID_HANDLE;
#endif

	struct Stat {
		int64_t size;
		std::filesystem::file_time_type ctime;
		std::filesystem::file_time_type mtime;
	};

	File() = default;
	File(File &&other) noexcept;
	File &operator=(File &&other) noexcept;
	~File();

	File(const File &) = delete;
	File &operator=(const File &) = delete;

	// Return information about the opened file.
	// Note that due to the nature of this call it can be outdated immediately
	// after the call if there are concurrent operations on the file.
	//
	// This is unlikely to incur disk I/O (it's expected that file metadata
	// is already in the memory cache) but it is a syscall nonetheless.
	//
	// Errors in this function are not expected ever, but if they do happen
	// it throws `voxen::Exception` with the respective error condition.
	Stat stat();

	// Link ("materialize") a temporary file handle to a name. Note that
	// this is very likely to fail if `path` is on another filesystem, so `path`
	// should usually be in the same directory that was passed to `open()`.
	// Also, you should not call it for non-temporary files, as then the behavior
	// is unspecified (it will either create a hard link or rename the file).
	//
	// Delete-on-close behavior of `FileFlagsBit::TempFile` will be disabled.
	// Use this to implement "atomic" file writes, where the file appears visible
	// in the filesystem (semantically, that is) only in fully "complete" state.
	//
	// On error, throws `voxen::Exception` with the respective error condition.
	void materializeTempFile(const std::filesystem::path &path);

	// Synchronous (blocking) read from the current file offset.
	// Returns the number of bytes read into `buffer`, file offset
	// is updated accordingly. If less bytes than `buffer.size()`
	// were read, this means the end of file was reached.
	//
	// Behavior is undefined if `read()` or `write()` are called concurrently
	// on this handle, or if the same file is concurrently written to overlapping
	// range (what is "overlapping" can't be known exactly with this interface).
	// Concurrent `pread()` calls are valid.
	//
	// On error, throws `voxen::Exception` with the respective error condition.
	// On Windows, behavior is undefined if the file was opened with `AsyncIo` flag.
	size_t read(std::span<std::byte> buffer);

	// Synchronous (blocking) read from the specified non-negative offset.
	// Returns the number of bytes read into `buffer`, file offset
	// is not affected. If less bytes than `buffer.size()`
	// were read, this means the end of file was reached.
	//
	// Behavior is undefined if the same file is concurrently written to
	// overlapping range from other threads/processes. Concurrent calls to
	// `read()`/`pread()` or non-overlapping `write()`/`pwrite()` are valid.
	//
	// On error, throws `voxen::Exception` with the respective error condition.
	// On Windows, behavior is undefined if the file was opened with `AsyncIo` flag.
	size_t pread(std::span<std::byte> buffer, int64_t offset);

	// Synchronous (blocking) write to the current file offset.
	// It either writes all bytes supplied in `buffer` (note that "writes" merely means
	// OS has acknowledged this request, not that data has fully reached the disk),
	// or writes an unspecified number of bytes and fails if an error happens.
	// File offset is updated according to the number of bytes written.
	//
	// Behavior is undefined if `read()` or `write()` are called concurrently
	// on this handle, or if the same file is concurrently written to overlapping
	// range (what is "overlapping" can't be known exactly with this interface).
	// Concurrent `pread()`/`pwrite()` calls are valid.
	//
	// On error, throws `voxen::Exception` with the respective error condition.
	// On Windows, behavior is undefined if the file was opened with `AsyncIo` flag.
	void write(std::span<const std::byte> buffer);

	// Synchronous (blocking) write to the specified non-negative offset.
	// It either writes all bytes supplied in `buffer` (note that "writes" merely means
	// OS has acknowledged this request, not that data has fully reached the disk),
	// or writes an unspecified number of bytes and fails if an error happens.
	// File offset is not affected.
	//
	// Behavior is undefined if the same file is concurrently written to
	// overlapping range from other threads/processes. Concurrent calls to
	// `read()`/`pread()` or non-overlapping `write()`/`pwrite()` are valid.
	//
	// On error, throws `voxen::Exception` with the respective error condition.
	// On Windows, behavior is undefined if the file was opened with `AsyncIo` flag.
	void pwrite(std::span<const std::byte> buffer, int64_t offset);

	NativeHandle get() const noexcept { return m_handle; }
	bool valid() const noexcept { return m_handle != INVALID_HANDLE; }

	// Tries to open (or create) a file, see description of `FileFlagsBit`
	// to find which open modes are available.
	//
	// On error, throws `voxen::Exception` with the respective error condition.
	//
	// Note that this is likely to incur (rather small) disk I/O if file
	// metadata is not in memory cache, and is generally not the fastest syscall.
	// This function might be worth offloading to a dedicated I/O thread.
	static File open(const std::filesystem::path &path, FileFlags flags);

	// Same as `open()` but returns result object instead of throwing.
	// Use this to avoid costly throw operation if having this file is not critical
	// for your further operation and therefore you can meaningfully react on an error.
	static cpp::result<File, std::error_condition> tryOpen(const std::filesystem::path &path, FileFlags flags);

	// Collect information about a file without opening it.
	// Note that this is likely to incur (rather small) disk I/O
	// if file metadata is not in memory cache.
	static cpp::result<Stat, std::error_condition> stat(const std::filesystem::path &path);

private:
	explicit File(NativeHandle native) noexcept;

	NativeHandle m_handle = INVALID_HANDLE;
};

} // namespace voxen::os
