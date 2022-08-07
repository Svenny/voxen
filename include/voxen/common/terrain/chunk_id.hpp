#pragma once

#include <compare>
#include <cstdint>
#include <functional>

namespace voxen::terrain
{

// This struct uniquely identifies a single chunk.
// Chunk-space is world-space divided by the smallest chunk size (see `Chunk::SIZE`).
struct alignas(uint64_t) ChunkId {
	// Chunk's LOD level. It's size is scaled by (1 << lod).
	uint32_t lod;
	// Smallest X coordinate of the chunk in chunk-space
	int32_t base_x;
	// Smallest Y coordinate of the chunk in chunk-space
	int32_t base_y;
	// Smallest Z coordinate of the chunk in chunk-space
	int32_t base_z;

	auto operator <=> (const ChunkId &) const noexcept = default;

	// Very fast hash function which can have worse distribution than `slowHash()`
	uint64_t fastHash() const noexcept;
	// Slower but less collision-prone hash function
	uint64_t slowHash() const noexcept;

	// Returns ID of the parent node. Results are undefined `lod` is greater than 30.
	ChunkId toParent() const noexcept;
	// Returns ID of the given child chunk. Results are undefined if
	// `lod` is zero or `id` is greater than or equal to 8.
	ChunkId toChild(size_t id) const noexcept;
};

}

namespace std
{

// Specialization of `std::hash` for `ChunkId`
template<>
struct hash<voxen::terrain::ChunkId> final {
public:
	size_t operator()(const voxen::terrain::ChunkId &id) const noexcept
	{
		return id.fastHash();
	}
};

}
