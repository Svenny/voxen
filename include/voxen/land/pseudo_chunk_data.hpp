#pragma once

#include <extras/dyn_array.hpp>

namespace voxen::land
{

struct ChunkAdjacencyRef;

class PseudoChunkData {
public:
	struct Face {
		uint32_t x : 5;
		uint32_t y : 5;
		uint32_t z : 5;
		uint32_t orientation : 3;
		uint32_t _unused : 14;
		uint32_t corner_weights;
		uint32_t color_packed_srgb;
	};

	PseudoChunkData() = default;
	// Generate pseudo-chunk data from chunk+adjacency references
	explicit PseudoChunkData(ChunkAdjacencyRef ref);
	// Generate lower resolution pseudo-chunk by aggregating 2x2x2 higher-resolution ones
	explicit PseudoChunkData(std::span<const PseudoChunkData *, 8> hires);

	bool empty() const noexcept { return m_faces.empty(); }

	const auto &faces() const noexcept { return m_faces; }

private:
	extras::dyn_array<Face> m_faces;
};

} // namespace voxen::land
