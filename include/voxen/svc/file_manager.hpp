#pragma once

#include <voxen/visibility.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Whole-file I/O relative to the game data and user profile directories.
// Errors are logged, functions return null/false on failure.
// Writes create missing directories and never leave partially written files.
namespace voxen::svc::FileManager
{

VOXEN_API void setProfileName(const std::string &path_to_binary, const std::string &profile_name = "default");

VOXEN_API std::optional<std::vector<std::byte>> readUserFile(const std::filesystem::path &relative_path);
VOXEN_API bool writeUserFile(const std::filesystem::path &relative_path, std::span<const std::byte> data);
VOXEN_API std::optional<std::string> readUserTextFile(const std::filesystem::path &relative_path);
VOXEN_API bool writeUserTextFile(const std::filesystem::path &relative_path, const std::string &text);

VOXEN_API std::optional<std::vector<std::byte>> readFile(const std::filesystem::path &relative_path);
VOXEN_API std::optional<std::string> readTextFile(const std::filesystem::path &relative_path);

VOXEN_API const std::filesystem::path &userDataPath() noexcept;
VOXEN_API const std::filesystem::path &gameDataPath() noexcept;

} // namespace voxen::svc::FileManager
