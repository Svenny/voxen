#include <tk/fs.hpp>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <system_error>
#include <utility>

#ifndef _WIN32
	#include <sys/stat.h>
#else
	#include <io.h>
#endif

namespace tk::fs
{

namespace
{

struct FileCloser {
	void operator()(FILE *file) const noexcept { fclose(file); }
};

using FilePtr = std::unique_ptr<FILE, FileCloser>;

FilePtr openFile(const std::filesystem::path &path, bool write) noexcept
{
#ifndef _WIN32
	return FilePtr(fopen(path.c_str(), write ? "wb" : "rb"));
#else
	return FilePtr(_wfopen(path.c_str(), write ? L"wb" : L"rb"));
#endif
}

// Returns negative value (likely setting errno) on error
int64_t fileSize(FILE *file) noexcept
{
#ifndef _WIN32
	struct stat file_stat;
	if (fstat(fileno(file), &file_stat) != 0) {
		return -1;
	}

	return static_cast<int64_t>(file_stat.st_size);
#else
	return _filelengthi64(_fileno(file));
#endif
}

std::string describeError(std::string_view what, const std::filesystem::path &path, std::error_code ec)
{
	return std::string(what) + " '" + path.string() + "': " + ec.message();
}

std::string describeErrno(std::string_view what, const std::filesystem::path &path)
{
	return describeError(what, path, std::error_code(errno, std::generic_category()));
}

template<typename T>
std::expected<T, std::string> readWhole(const std::filesystem::path &path)
{
	FilePtr file = openFile(path, false);
	if (!file) {
		return std::unexpected(describeErrno("can't open file", path));
	}

	const int64_t size = fileSize(file.get());
	if (size < 0) {
		return std::unexpected(describeErrno("can't get size of file", path));
	}

	T storage(static_cast<size_t>(size), {});
	const size_t read = fread(storage.data(), 1, storage.size(), file.get());
	if (read != storage.size()) {
		return std::unexpected(describeErrno("can't read file", path) + " (" + std::to_string(read) + "/"
			+ std::to_string(storage.size()) + " bytes read)");
	}

	return storage;
}

} // namespace

std::expected<std::vector<std::byte>, std::string> readFile(const std::filesystem::path &path)
{
	return readWhole<std::vector<std::byte>>(path);
}

std::expected<std::string, std::string> readTextFile(const std::filesystem::path &path)
{
	return readWhole<std::string>(path);
}

std::expected<void, std::string> writeFile(const std::filesystem::path &path, std::span<const std::byte> data)
{
	std::error_code ec;

	if (path.has_parent_path()) {
		std::filesystem::create_directories(path.parent_path(), ec);
		if (ec) {
			return std::unexpected(describeError("can't create directories for file", path, ec));
		}
	}

	std::filesystem::path temp_path = path;
	temp_path += ".write-tmp";

	FilePtr file = openFile(temp_path, true);
	if (!file) {
		return std::unexpected(describeErrno("can't open temporary file", temp_path));
	}

	const size_t written = fwrite(data.data(), 1, data.size(), file.get());
	// Close before renaming, buffered data can fail to write at this point too
	const bool close_failed = fclose(file.release()) != 0;

	if (written != data.size() || close_failed) {
		std::string error = describeErrno("can't write file", temp_path);
		std::filesystem::remove(temp_path, ec);
		return std::unexpected(std::move(error));
	}

	std::filesystem::rename(temp_path, path, ec);
	if (ec) {
		std::string error = describeError("can't rename temporary file to", path, ec);
		std::filesystem::remove(temp_path, ec);
		return std::unexpected(std::move(error));
	}

	return {};
}

std::expected<void, std::string> writeTextFile(const std::filesystem::path &path, std::string_view text)
{
	return writeFile(path, std::as_bytes(std::span(text)));
}

} // namespace tk::fs
