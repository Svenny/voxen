#pragma once

#include <voxen/os/file.hpp>
#include <voxen/svc/service_base.hpp>
#include <voxen/visibility.hpp>

#include <cpp/result.hpp>

#include <filesystem>

namespace voxen::svc
{

// Provides higher-level file operations than raw `os::File`.
class VOXEN_API FileService final : public IService {
public:
	constexpr static UID SERVICE_UID = UID("91131570-ddfb7ba3-49b63d4d-04aaf4c8");

	using OpenResult = os::File::OpenResult;

	struct Config {
		std::filesystem::path data_root;
		std::filesystem::path user_root;
	};

	FileService(Config cfg);
	FileService(FileService &&) = delete;
	FileService(const FileService &) = delete;
	FileService &operator=(FileService &&) = delete;
	FileService &operator=(const FileService &) = delete;
	~FileService() override;

	UID serviceUid() const noexcept override { return SERVICE_UID; }

	// Opens a file in data directory (`Config::data_root`). For safety reasons `path` must be relative
	// and stay inside the directory (no "../" backlinks), otherwise `FileError::InvalidPath` will be returned.
	//
	// As this is engine runtime service (not intended for toolchain use) data is intended
	// to always be read-only. So file open mode is `Read` and create mode is `None`.
	OpenResult openDataFile(const std::filesystem::path &relative_path);

	// Opens a file in user profile directory (`Config::user_root`). For safety reasons `path` must be relative
	// and stay inside the directory (no "../" backlinks), otherwise `FileError::InvalidPath` will be returned.
	//
	// Open and create mode can be specified as user files are intended to be both read and written.
	OpenResult openUserFile(const std::filesystem::path &relative_path, os::FileOpenMode open_mode,
		os::FileCreateMode create_mode = os::FileCreateMode::None);

	// Creates an empty temporary file for writing. This file will automatically delete on close or process exit
	// unless it gets linked to some path in the user root. Use this function to implement atomic file writes.
	//
	// This file is located in an unspecified place in either user or platform-dependent temp directory.
	OpenResult createTemporaryFile();

private:
	Config m_cfg;
};

} // namespace voxen::svc
