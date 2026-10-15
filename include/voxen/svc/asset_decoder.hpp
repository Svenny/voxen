#pragma once

#include <voxen/svc/asset_base.hpp>
#include <voxen/svc/svc_fwd.hpp>

#include <expected>
#include <span>
#include <system_error>

namespace voxen::svc
{

class IAssetDecoder {
public:
	struct BeginInfo {
		AssetService &service;
		AssetStorageCode storage_code;
		AssetFileRef file_ref;
		size_t file_size;
	};

	struct Deleter {
		void operator()(IAssetDecoder *ptr) noexcept;
	};

	// Points to pipe memory allocation, must be created only with `PipeMemoryAllocator::make()`
	using PipePtr = std::unique_ptr<IAssetDecoder, Deleter>;

	virtual ~IAssetDecoder();

	virtual std::expected<size_t, std::error_code> beginDecode(BeginInfo info) = 0;
	virtual std::expected<size_t, std::error_code> receiveBytes(std::span<const std::byte> bytes) = 0;
	virtual std::expected<AssetBasePtr, std::error_code> endDecode() = 0;
};

} // namespace voxen::svc
