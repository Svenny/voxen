#include <voxen/common/terrain/chunk_id.hpp>

#include <voxen/util/hash.hpp>

#include <cassert>
#include <cstring>

namespace voxen::terrain
{

uint64_t ChunkId::fastHash() const noexcept
{
	static_assert(sizeof(ChunkId) == 2 * sizeof(uint64_t));

	uint64_t vals[2];
	memcpy(vals, this, sizeof(ChunkId));
	return vals[0] ^ (vals[1] * uint64_t(0xDEADBEEF));
}

uint64_t ChunkId::slowHash() const noexcept
{
	return hashFnv1a(this, sizeof(ChunkId));
}

ChunkId ChunkId::toParent() const noexcept
{
	assert(this->lod < 31u);

	// This mask is ANDed with base coordinates to disable one bit
	const auto offmask = ~int32_t(1u << this->lod);

	return ChunkId {
		.lod = this->lod + 1u,
		.base_x = this->base_x & offmask,
		.base_y = this->base_y & offmask,
		.base_z = this->base_z & offmask
	};
}

ChunkId ChunkId::toChild(unsigned id) const noexcept
{
	assert(id < 8u);
	assert(this->lod > 0u);

	// This mask is ORed with base coordinates to enable one bit
	const auto onmask = int32_t(1u << (this->lod - 1u));

	return ChunkId {
		.lod = this->lod - 1u,
		.base_x = this->base_x | ((id & 1) ? onmask : 0),
		.base_y = this->base_y | ((id & 2) ? onmask : 0),
		.base_z = this->base_z | ((id & 4) ? onmask : 0)
	};
}

}
