#include <voxen/os/file.hpp>

#include <extras/defer.hpp>

#include "../../voxen_test_common.hpp"

namespace voxen::os
{

namespace
{

constexpr char TEST_TEXT_1[] = "Test\nText\n";
constexpr size_t TEST_TEXT_1_LEN = std::size(TEST_TEXT_1) - 1;

} // namespace

TEST_CASE("'File' test case 1", "[voxen::os::file]")
{
	std::filesystem::path tmp_path = std::filesystem::temp_directory_path() / "test-voxen-file-case1";
	INFO("Temporary directory: " << tmp_path);

	REQUIRE_NOTHROW(std::filesystem::create_directory(tmp_path));
	// Clean up any mess after test sections
	defer { std::filesystem::remove_all(tmp_path); };

	SECTION("Write a file, then read it back through a different handle")
	{
		std::filesystem::path file_path = tmp_path / "file1.txt";
		INFO("File path: " << file_path);

		REQUIRE(!std::filesystem::exists(file_path));

		std::filesystem::file_time_type last_ctime;
		std::filesystem::file_time_type last_mtime;

		{
			FileFlags flags = { FileFlagsBit::LockExclusive, FileFlagsBit::Write, FileFlagsBit::Create };

			File write_file;
			REQUIRE_NOTHROW(write_file = File::open(file_path, flags));
			CHECK(write_file.valid());

			File::Stat file_stat;
			CHECK_NOTHROW(file_stat = write_file.stat());
			CHECK(file_stat.size == 0);

			CHECK_NOTHROW(write_file.write(std::as_bytes(std::span(TEST_TEXT_1, TEST_TEXT_1_LEN))));

			CHECK_NOTHROW(file_stat = write_file.stat());
			last_ctime = file_stat.ctime;
			last_mtime = file_stat.mtime;
			CHECK(last_ctime <= last_mtime);
		}

		{
			FileFlags flags = { FileFlagsBit::Read, FileFlagsBit::LockShared };

			File read_file;
			REQUIRE_NOTHROW(read_file = File::open(file_path, flags));
			CHECK(read_file.valid());

			char out_text[TEST_TEXT_1_LEN + 1] = {};
			size_t read_bytes;
			CHECK_NOTHROW(read_bytes = read_file.read(std::as_writable_bytes(std::span(out_text, TEST_TEXT_1_LEN))));
			CHECK(read_bytes == TEST_TEXT_1_LEN);

			CHECK(std::string_view(TEST_TEXT_1) == std::string_view(out_text));

			File::Stat file_stat;
			CHECK_NOTHROW(file_stat = read_file.stat());
			CHECK(file_stat.size == TEST_TEXT_1_LEN);
			CHECK(last_ctime == file_stat.ctime);
			CHECK(last_mtime == file_stat.mtime);
		}
	}

	SECTION("Create a temporary read/write file")
	{
		{
			FileFlags flags { FileFlagsBit::Read, FileFlagsBit::Write, FileFlagsBit::TempFile };

			File rw_file;
			REQUIRE_NOTHROW(rw_file = File::open(tmp_path, flags));

			CHECK_NOTHROW(rw_file.pwrite(std::as_bytes(std::span(TEST_TEXT_1, TEST_TEXT_1_LEN)), 0));

			char out_text[TEST_TEXT_1_LEN + 1] = {};
			size_t read_bytes;
			CHECK_NOTHROW(read_bytes = rw_file.pread(std::as_writable_bytes(std::span(out_text, TEST_TEXT_1_LEN)), 0));
			CHECK(read_bytes == TEST_TEXT_1_LEN);

			CHECK(std::string_view(TEST_TEXT_1) == std::string_view(out_text));

			File::Stat file_stat;
			CHECK_NOTHROW(file_stat = rw_file.stat());
			CHECK(file_stat.size == TEST_TEXT_1_LEN);
		}

		std::filesystem::directory_iterator dir(tmp_path);
		// File should be automatically deleted after closing
		// so our temporary directory must be empty
		CHECK(dir == std::filesystem::directory_iterator());
	}

	SECTION("Create a temporary file, then materialize it")
	{
		std::filesystem::path file_path = tmp_path / "file1.txt";
		INFO("File path: " << file_path);

		REQUIRE(!std::filesystem::exists(file_path));

		{
			FileFlags flags { FileFlagsBit::Write, FileFlagsBit::TempFile };

			File write_file;
			REQUIRE_NOTHROW(write_file = File::open(tmp_path, flags));
			CHECK_NOTHROW(write_file.write(std::as_bytes(std::span(TEST_TEXT_1, TEST_TEXT_1_LEN))));
			CHECK_NOTHROW(write_file.materializeTempFile(file_path));
		}

		{
			FileFlags flags { FileFlagsBit::Read };

			File read_file;
			REQUIRE_NOTHROW(read_file = File::open(file_path, flags));

			char out_text[TEST_TEXT_1_LEN + 1] = {};
			size_t read_bytes;
			CHECK_NOTHROW(read_bytes = read_file.read(std::as_writable_bytes(std::span(out_text, TEST_TEXT_1_LEN))));
			CHECK(read_bytes == TEST_TEXT_1_LEN);

			CHECK(std::string_view(TEST_TEXT_1) == std::string_view(out_text));
		}
	}
}

TEST_CASE("'File' test case 2", "[voxen::os::file]")
{
	std::filesystem::path tmp_path = std::filesystem::temp_directory_path() / "test-voxen-file-case2";
	INFO("Temporary directory: " << tmp_path);

	REQUIRE_NOTHROW(std::filesystem::create_directory(tmp_path));
	// Clean up any mess after test sections
	defer { std::filesystem::remove_all(tmp_path); };

	SECTION("Invalid open calls")
	{
		std::filesystem::path path = tmp_path / "file1.txt";
		INFO("File path: " << path);

		CHECK_THROWS_MATCHES(File::open(path, { FileFlagsBit::Read }), Exception,
			test::errcExceptionMatcher(std::errc::no_such_file_or_directory));

		CHECK_THROWS_MATCHES(File::open(path, { FileFlagsBit::Write }), Exception,
			test::errcExceptionMatcher(std::errc::no_such_file_or_directory));

		CHECK_THROWS_MATCHES(File::open(path,
										{ FileFlagsBit::Write, FileFlagsBit::LockShared, FileFlagsBit::LockExclusive }),
			Exception, test::errcExceptionMatcher(std::errc::invalid_argument));

		CHECK_THROWS_MATCHES(
			File::open(path, { FileFlagsBit::Read, FileFlagsBit::HintRandomAccess, FileFlagsBit::HintSequentialAccess }),
			Exception, test::errcExceptionMatcher(std::errc::invalid_argument));
	}

	SECTION("Invalid stat calls")
	{
		std::filesystem::path path = tmp_path / "file1.txt";
		INFO("File path: " << path);

		auto stat_result = File::stat(path);
		CHECK(stat_result.has_error());
		CHECK(stat_result.error() == std::errc::no_such_file_or_directory);
	}
}

} // namespace voxen::os
