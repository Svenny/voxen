#pragma once

#include <cstdint>

namespace voxen::assets
{

enum class AssetType {
	Blob,
	Text,
	Texture,
};

struct AssetDescriptor {
	uint64_t id;
	uint64_t hash;
	AssetType type;
};

} // namespace voxen::assets
