#include "../test_common.hpp"

#include <array>
#include <filesystem>

import tk.fs;

namespace tk::fs
{

namespace
{

// Creates a fresh temporary directory, removes it with all contents on destruction
struct TempDir {
	explicit TempDir(const char *name) : path(std::filesystem::temp_directory_path() / name)
	{
		std::filesystem::remove_all(path);
		std::filesystem::create_directory(path);
	}

	~TempDir()
	{
		std::error_code ec;
		std::filesystem::remove_all(path, ec);
	}

	TempDir(TempDir &&) = delete;
	TempDir(const TempDir &) = delete;
	TempDir &operator=(TempDir &&) = delete;
	TempDir &operator=(const TempDir &) = delete;

	std::filesystem::path path;
};

} // namespace

TEST_CASE("'fs' writes and reads back files", "[tk::fs]")
{
	TempDir dir("test-tk-fs-case1");
	INFO("Temporary directory: " << dir.path);

	const auto bytes = std::to_array({ std::byte(0), std::byte(0x0A), std::byte(0xFF), std::byte(0x0D) });

	SECTION("Binary file in missing subdirectories")
	{
		const std::filesystem::path file = dir.path / "a" / "b" / "file.bin";
		REQUIRE(writeFile(file, bytes).has_value());

		auto read = readFile(file);
		REQUIRE(read.has_value());
		CHECK(std::ranges::equal(*read, bytes));

		// No temporary files are left behind
		CHECK(std::distance(std::filesystem::directory_iterator(file.parent_path()), {}) == 1);
	}

	SECTION("Text file, overwritten")
	{
		const std::filesystem::path file = dir.path / "file.txt";
		REQUIRE(writeTextFile(file, "first version\nwith two lines\n").has_value());
		REQUIRE(writeTextFile(file, "second\r\n").has_value());

		auto read = readTextFile(file);
		REQUIRE(read.has_value());
		CHECK(*read == "second\r\n");
	}

	SECTION("Empty file")
	{
		const std::filesystem::path file = dir.path / "empty";
		REQUIRE(writeFile(file, {}).has_value());

		auto read = readFile(file);
		REQUIRE(read.has_value());
		CHECK(read->empty());
	}
}

TEST_CASE("'fs' reports errors", "[tk::fs]")
{
	TempDir dir("test-tk-fs-case2");
	INFO("Temporary directory: " << dir.path);

	auto read = readFile(dir.path / "missing.bin");
	REQUIRE_FALSE(read.has_value());
	// The message mentions the path
	CHECK(read.error().find("missing.bin") != std::string::npos);

	// Parent "directory" is a regular file
	REQUIRE(writeTextFile(dir.path / "file", "").has_value());
	CHECK_FALSE(writeTextFile(dir.path / "file" / "nested", "").has_value());
}

} // namespace tk::fs
