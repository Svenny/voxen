#pragma once

#include <cpp/result.hpp>

#include <filesystem>
#include <system_error>

namespace voxen::os
{

enum class FileOpenMode {
	Read,
	Write,
	Append,
};

enum class FileCreateMode {
	None,
	File,
	FileDirs,
};

enum class FileError {
	Success,
	InvalidPath,
};

class File {
public:
	using OpenResult = cpp::result<File, std::error_code>;


	static OpenResult open(const std::filesystem::path &path, FileOpenMode open_mode, FileCreateMode create_mode);

private:
};

}
