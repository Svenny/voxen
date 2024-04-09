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

	static std::optional<extras::dyn_array<std::byte>> readUserFile(const std::filesystem::path& relative_path) noexcept;
	static bool writeUserFile(const std::filesystem::path& relative_path, const void* data, size_t size,
		bool create_directories = true) noexcept;
	static std::optional<std::string> readUserTextFile(const std::filesystem::path& relative_path) noexcept;
	static bool writeUserTextFile(const std::filesystem::path& relative_path, const std::string& text,
		bool create_directories = true) noexcept;

	static std::optional<extras::dyn_array<std::byte>> readFile(const std::filesystem::path& relative_path) noexcept;
	static std::optional<std::string> readTextFile(const std::filesystem::path& relative_path) noexcept;

	static std::future<std::optional<extras::dyn_array<std::byte>>> readUserFileAsync(
		std::filesystem::path relative_path) noexcept;
	static std::future<bool> writeUserFileAsync(std::filesystem::path relative_path, const void* data, size_t size,
		bool create_directories = true) noexcept;
	static std::future<std::optional<std::string>> readUserTextFileAsync(std::filesystem::path relative_path) noexcept;
	static std::future<bool> writeUserTextFileAsync(std::filesystem::path relative_path, const std::string&& text,
		bool create_directories = true) noexcept;

	static std::future<std::optional<extras::dyn_array<std::byte>>> readFileAsync(
		std::filesystem::path relative_path) noexcept;
	static std::future<std::optional<std::string>> readTextFileAsync(std::filesystem::path relative_path) noexcept;

	static bool makeDirsForFile(const std::filesystem::path& relative_path) noexcept;

	static std::filesystem::path userDataPath() noexcept;
	static std::filesystem::path gameDataPath() noexcept;

private:
	static std::filesystem::path user_data_path;
	static std::filesystem::path game_data_path;
};
} // namespace voxen
