#pragma once

#include <cstring>
#include <optional>
#include <future>
#include <filesystem>

#include <extras/dyn_array.hpp>

namespace voxen
{

class FileManager {
public:
	static void setProfileName(const std::string& profile_name);

	static std::optional<extras::dyn_array<std::byte>> readUserFile(const std::filesystem::path& relative_path) noexcept;
	static bool writeUserFile(const std::filesystem::path& relative_path, const void *data, size_t size) noexcept;
	static std::optional<std::string> readUserTextFile(const std::filesystem::path& relative_path) noexcept;
	static bool writeUserTextFile(const std::filesystem::path& relative_path, const std::string& text) noexcept;

	static std::optional<extras::dyn_array<std::byte>> readFile(const std::filesystem::path& relative_path) noexcept;
	static std::optional<std::string> readTextFile(const std::filesystem::path& relative_path) noexcept;

	static std::future<std::optional<extras::dyn_array<std::byte>>> readUserFileAsync(const std::filesystem::path& relative_path) noexcept;
	static std::future<bool> writeUserFileAsync(const std::filesystem::path& relative_path, const void *data, size_t size) noexcept;
	static std::future<std::optional<std::string>> readUserTextFileAsync(const std::filesystem::path& relative_path) noexcept;
	static std::future<bool> writeUserTextFileAsync(const std::filesystem::path& relative_path, const std::string& text) noexcept;

	static std::future<std::optional<extras::dyn_array<std::byte>>> readFileAsync(const std::filesystem::path& relative_path) noexcept;
	static std::future<std::optional<std::string>> readTextFileAsync(const std::filesystem::path& relative_path) noexcept;

	static bool makeDirsForFile(const std::filesystem::path& relative_path) noexcept;

	static std::filesystem::path userDataPath() noexcept;
	static std::filesystem::path gameDataPath() noexcept;

private:
	static std::string profile_name;
};
}
