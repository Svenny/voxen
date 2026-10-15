#include <voxen/common/filemanager.hpp>

#include <voxen/os/stdlib.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <fmt/std.h>

#include <system_error>

using extras::dyn_array;
using std::filesystem::path;
using std::optional;
using std::string;

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

} // anonymous namespace

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

} // namespace voxen
