#include <voxen/util/file.hpp>

#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <cstring>
#include <string>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace voxen
{

static void printErrno(const char *message, const char *path) noexcept {
	int code = errno;
	char buf[1024];
	char *desc = strerror_r(code, buf, std::size(buf));
	Log::warn("{} `{}`, error code {} ({})", message, path, code, desc);
}

std::optional<extras::dyn_array<std::byte>> FileUtils::readFile(const char *path) noexcept {
	if (!path)
		return std::nullopt;

	// TODO: handle EINTR
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		printErrno("Can't open file", path);
		return std::nullopt;
	}
	defer { close(fd); };

	struct stat file_stat;
	if (fstat(fd, &file_stat) != 0) {
		printErrno("Can't stat file", path);
		return std::nullopt;
	}

	size_t size = size_t(file_stat.st_size);
	try {
		extras::dyn_array<std::byte> data(size);
		// TODO: handle EINTR
		if (read(fd, data.data(), size) < 0) {
			printErrno("Can't read file", path);
			return std::nullopt;
		}
		return data;
	}
	catch (const std::bad_alloc &e) {
		Log::warn("Out of memory: {}", e.what());
		return std::nullopt;
	}
}

bool FileUtils::writeFile(const char *path, const void *data, size_t size) noexcept {
	std::string temp_path_s;
	try {
		temp_path_s = path;
		temp_path_s += ".XXXXXX";
	}
	catch (const std::bad_alloc &e) {
		Log::warn("Out of memory: {}", e.what());
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
		Log::warn("{}/{} bytes written", written, size);
		return false;
	}

	if (rename(temp_path, path) != 0) {
		printErrno("Can't rename file", temp_path);
		return false;
	}

	return true;
}

}
