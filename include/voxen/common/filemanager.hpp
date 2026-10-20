#pragma once

#include <voxen/visibility.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace voxen
{

// Whole-file I/O relative to the game data and user profile directories.
// Errors are logged, functions return null/false on failure.
// Writes create missing directories and never leave partially written files.
class VOXEN_API FileManager {
public:
	static void setProfileName(const std::string &path_to_binary, const std::string &profile_name = "default");

	static std::optional<std::vector<std::byte>> readUserFile(const std::filesystem::path &relative_path);
	static bool writeUserFile(const std::filesystem::path &relative_path, std::span<const std::byte> data);
	static std::optional<std::string> readUserTextFile(const std::filesystem::path &relative_path);
	static bool writeUserTextFile(const std::filesystem::path &relative_path, const std::string &text);

	static std::optional<std::vector<std::byte>> readFile(const std::filesystem::path &relative_path);
	static std::optional<std::string> readTextFile(const std::filesystem::path &relative_path);

	static const std::filesystem::path &userDataPath() noexcept { return user_data_path; }
	static const std::filesystem::path &gameDataPath() noexcept { return game_data_path; }

private:
	static std::filesystem::path user_data_path;
	static std::filesystem::path game_data_path;
};

} // namespace voxen
