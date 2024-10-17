#pragma once

#include <voxen/visibility.hpp>

#include <extras/dyn_array.hpp>

#include <cstring>
#include <filesystem>
#include <future>
#include <optional>

namespace voxen
{

class VOXEN_API FileManager {
public:
	static void setProfileName(const std::string& path_to_binary, const std::string& profile_name = "default");

	static std::optional<extras::dyn_array<std::byte>> readUserFile(const std::filesystem::path& relative_path);
	static bool writeUserFile(const std::filesystem::path& relative_path, std::span<const std::byte> data,
		bool create_directories = true);
	static std::optional<std::string> readUserTextFile(const std::filesystem::path& relative_path);
	static bool writeUserTextFile(const std::filesystem::path& relative_path, const std::string& text,
		bool create_directories = true);

	static std::optional<extras::dyn_array<std::byte>> readFile(const std::filesystem::path& relative_path);
	static std::optional<std::string> readTextFile(const std::filesystem::path& relative_path);

	static std::future<std::optional<extras::dyn_array<std::byte>>> readUserFileAsync(
		std::filesystem::path relative_path);
	static std::future<std::optional<std::string>> readUserTextFileAsync(std::filesystem::path relative_path);
	static std::future<bool> writeUserTextFileAsync(std::filesystem::path relative_path, std::string text,
		bool create_directories = true);

	static std::future<std::optional<extras::dyn_array<std::byte>>> readFileAsync(std::filesystem::path relative_path);
	static std::future<std::optional<std::string>> readTextFileAsync(std::filesystem::path relative_path);

	static bool makeDirsForFile(const std::filesystem::path& relative_path);

	static const std::filesystem::path& userDataPath() noexcept { return user_data_path; }
	static const std::filesystem::path& gameDataPath() noexcept { return game_data_path; }

private:
	static std::filesystem::path user_data_path;
	static std::filesystem::path game_data_path;
};
} // namespace voxen
