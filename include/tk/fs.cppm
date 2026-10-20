module;

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module tk.fs;

// Synchronous whole-file I/O helpers.
// Errors are returned as messages including the path and the OS error description.

export namespace tk::fs
{

// Read the whole file contents
std::expected<std::vector<std::byte>, std::string> readFile(const std::filesystem::path &path);
// Read the whole file contents as text, without any newline or encoding conversion
std::expected<std::string, std::string> readTextFile(const std::filesystem::path &path);

// Replace the file contents, creating missing parent directories. The data is written to
// a temporary file next to `path` which is then renamed to it, so other readers will see
// either the old or the new contents, but never a partially written file.
std::expected<void, std::string> writeFile(const std::filesystem::path &path, std::span<const std::byte> data);
// Same as `writeFile`, for text without any newline or encoding conversion
std::expected<void, std::string> writeTextFile(const std::filesystem::path &path, std::string_view text);

} // namespace tk::fs
