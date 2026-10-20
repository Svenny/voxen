#include <voxen/common/filemanager.hpp>

#include <voxen/util/log.hpp>

#include <tk/fs.hpp>

using std::filesystem::path;

namespace voxen
{

namespace
{

template<typename T>
std::optional<T> logError(std::expected<T, std::string> result)
{
	if (!result) {
		Log::error("{}", result.error());
		return std::nullopt;
	}

	return std::move(*result);
}

bool logError(const std::expected<void, std::string> &result)
{
	if (!result) {
		Log::error("{}", result.error());
		return false;
	}

	return true;
}

} // anonymous namespace

path FileManager::user_data_path;
path FileManager::game_data_path;

void FileManager::setProfileName(const std::string &path_to_binary, const std::string &profile_name)
{
	// Path to voxen binary is <install root>/bin/voxen, go two files above to get install root
	auto install_root = std::filesystem::path(path_to_binary).parent_path().parent_path();

	user_data_path = (install_root / "data" / "user" / profile_name).lexically_normal();
	game_data_path = (install_root / "data").lexically_normal();
}

std::optional<std::vector<std::byte>> FileManager::readUserFile(const path &relative_path)
{
	if (relative_path.empty()) {
		return std::nullopt;
	}

	return logError(tk::fs::readFile(userDataPath() / relative_path));
}

bool FileManager::writeUserFile(const path &relative_path, std::span<const std::byte> data)
{
	return logError(tk::fs::writeFile(userDataPath() / relative_path, data));
}

std::optional<std::string> FileManager::readUserTextFile(const path &relative_path)
{
	if (relative_path.empty()) {
		return std::nullopt;
	}

	return logError(tk::fs::readTextFile(userDataPath() / relative_path));
}

bool FileManager::writeUserTextFile(const path &relative_path, const std::string &text)
{
	return logError(tk::fs::writeTextFile(userDataPath() / relative_path, text));
}

std::optional<std::vector<std::byte>> FileManager::readFile(const path &relative_path)
{
	if (relative_path.empty()) {
		return std::nullopt;
	}

	return logError(tk::fs::readFile(gameDataPath() / relative_path));
}

std::optional<std::string> FileManager::readTextFile(const path &relative_path)
{
	if (relative_path.empty()) {
		return std::nullopt;
	}

	return logError(tk::fs::readTextFile(gameDataPath() / relative_path));
}

} // namespace voxen
