#pragma once

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/common/uid.hpp>
#include <voxen/svc/pipe_memory_function.hpp>

#include <array>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <system_error>

namespace voxen::svc
{

using AssetStorageCode = std::array<char, 4>;

class AssetFileRef {
public:
private:
};

enum class Priority {
	Low,
	Normal,
	High,
};

class IAsset {
public:
	virtual ~IAsset();

	virtual UID assetTypeUid() const noexcept = 0;

protected:
	AssetFileRef m_file_ref;
};

template<typename Asset>
using AssetPtr = std::shared_ptr<Asset>;

using AssetBasePtr = AssetPtr<IAsset>;

template<typename Asset>
AssetPtr<Asset> assetDynamicCast(AssetBasePtr base)
{
	if (!base || Asset::ASSET_TYPE_UID != base->assetTypeUid()) [[unlikely]] {
		return {};
	}

	Asset *cast_ptr = static_cast<Asset *>(base.get());
	return AssetPtr<Asset>(std::move(base), cast_ptr);
}

#if 0
struct AssetLoadRequest {
	UID uid;
	Priority priority = Priority::Normal;
};

struct AssetPartLoadRequest {
	AssetFileRef asset_file_ref;
	std::pair<size_t, size_t> byte_range;
	void *write_memory;
	Priority priority;
};

// For some fixed-location loads or debugging purposes
std::optional<UID> lookupAssetUidByPath(std::u8string_view asset_path);

// Asset object will be created or reused from shared storage
void loadAsset(AssetLoadRequest request, PipeMemoryFunction<void()> completer)
{
	// void *storage = pipe_alloc<TAssetDecoder>::allocate()
	// IAssetDecoder *decoder = asset.placementNewDecoder(storage);
	// submit_cmd(asset, decoder, ...) {
	// completion_fn(ctx, asset);
	// }
}

template<typename F>
void loadAssetPart(AssetPartLoadRequest request, F &&completion_fn)
{
	// enqueue memory range read + allocate if needed
	//
}
#endif

/*

class TextureAssetTraits {
public:
	using AssetType = TextureAsset;
	using AssetHandleType = SharedPoolPtr<TextureAsset>;
	using DecoderType = TextureAssetDecoder;

	AssetHandleType createAsset();

private:
};

class TextureAsset : public AssetBase {
public:
	using Traits = TextureAssetTraits;
	using MipBytes = extras::dyn_array<std::byte>;

	struct LoadMipRequest {
		AssetService &service;
		Priority priority;
		int32_t mip;
	};

	template<typename F>
	void loadMip(LoadMipRequest req, F &&completion_fn)
	{
		req.service.loadAssetPart({
			.asset_file_ref = this->getFileRef(),
			.byte_range = byteRangeForMip(req.mip),
			.write_memory = nullptr,
			.priority = req.priority,
		}, [mip = req.mip, fn = std::forward<F>(completion_fn)](std::expected<std::span<std::byte>, std::error_code> maybe_memory)) {
			fn(maybe_memory.map(decodeMipLevel));
		};
	}

private:
	int32_t m_width = 0;
	int32_t m_height = 0;
	int32_t m_depth_or_layers = 0;
	int32_t m_mips = 0;

	TextureType m_type;
	PixelFormat m_pixel_format;
	ColorSpace m_color_space;

	std::pair<size_t, size_t> byteRangeForMip(int32_t mip) const noexcept;
	std::expected<MipBytes, std::error_code> decodeMipLevel(int32_t mip);
};

void mycode()
{
	AssetService &asvc;
	TaskService &tsvc;

	asvc.registerAssetTraits<TextureAssetTraits>();

	// Calculate where this texture should be stored
	size_t my_texture_id;

	// Main asset loading
	asvc.loadWithCompletion<TextureAsset>({
		.uid = ASSET_UID,
		.priority = Priority::Normal,
	}, [snd = this->m_sender, texid = my_texture_id](AssetLoadResult<TextureAsset> result) {
		if (result.error()) {
			snd->send<TextureLoadFailed>(MY_UID, texid, result.error());
		} else {
			snd->send<TextureLoadedMessage>(MY_UID, texid, result.takeHandle());
		}
	});

	// Streaming is similar
	AssetHandle<TextureAsset> my_texture;

	my_texture->loadMip({
		.service = asvc,
		.priority = Priority::Normal,
		.mip = 0,
	}, [snd = this->m_sender, texid = my_texture_id](AssetPartLoadResult<TextureAsset::MipBytes> result) {
		if (result.error()) {
			snd->send<TextureMipStreamFailed>(MY_UID, texid, 0, result.error());
		} else {
			snd->send<TextureMipStreamMessage>(MY_UID, texid, 0, result.takeData());
		}
	});
}

*/

} // namespace voxen::svc
