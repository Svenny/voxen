#pragma once

#include <voxen/util/packed_color.hpp>

#include <extras/dyn_array.hpp>

namespace voxen::land
{

struct ChunkAdjacencyRef;

class FakeChunkData {
public:
	struct FakeFace {
		uint32_t x : 5;
		uint32_t y : 5;
		uint32_t z : 5;
		uint32_t face_index : 3;
		uint32_t color_array_index : 14;
		uint32_t corner_weights;
		// TODO: for simplicity&debug, use palette instead (`color_array_index`)
		uint32_t color_packed_srgb;
	};

	// Generate fake chunk data from chunk+adjacency references
	explicit FakeChunkData(ChunkAdjacencyRef ref);
	// Generate lower resolution fake chunk data by aggregating 2x2x2 higher-resolution ones
	explicit FakeChunkData(std::span<const FakeChunkData *, 8> hires);

	bool empty() const noexcept { return m_faces.empty(); }

	const auto &faces() const noexcept { return m_faces; }

private:
	extras::dyn_array<FakeFace> m_faces;
	extras::dyn_array<PackedColorSrgb> m_colors;
};

} // namespace voxen::land
