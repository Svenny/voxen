#pragma once

#include <voxen/land/land_fwd.hpp>
#include <voxen/land/qef_solver.hpp>

#include <extras/dyn_array.hpp>

#include <glm/vec3.hpp>

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
		uint32_t color_packed_srgb;
	};

	struct EdgeEntry {
		uint8_t x;
		uint8_t y;
		uint8_t z;

		uint8_t axis : 2;
		uint8_t normal_y_sign : 1;
		uint8_t lower_endpoint_solid : 1;
		uint8_t _unused : 4;

		uint16_t offset_unorm;
		int16_t normal_x_snorm;
		int16_t normal_z_snorm;
	};

	struct CellEntry {
		uint8_t x;
		uint8_t y;
		uint8_t z;

		uint8_t histogram_is_color_mask : 4;
		uint8_t _unused : 4;

		uint16_t histogram_mat_id_or_color[4];
		uint8_t histogram_mat_weight[4];
	};

	PseudoChunkData() = default;
	// Generate pseudo-chunk data from chunk+adjacency references
	explicit PseudoChunkData(ChunkAdjacencyRef ref);
	// Generate lower resolution pseudo-chunk by aggregating 2x2x2 higher-resolution ones
	explicit PseudoChunkData(std::span<const PseudoChunkData *, 8> hires);

	// Generate pseudo-chunk LOD1 data from 20 (8 + 3x4) LOD0 (true) chunks.
	// Arrangement of pointers in the array must be this:
	// - [0:8) - "primary" LOD0 chunks comprising the LOD1 cube in YXZ index order
	// - [8:12) - adjacent chunks from Z+ direction in YX index order
	// - [12:16) - adjacent chunks from X+ direction (east of "primary" ones) in YZ index order
	// - [16:20) - adjacent chunks from Y+ direction (above "primary" ones) in XZ index order
	//
	// All pointers must be valid, but can point to special dummy chunks.
	void generateFromLod0(std::span<const Chunk *const, 20> chunks);
	// Generate (aggregate) pseudo-chunk data for LOD(n) from 8 LOD(n-1)
	// pseudo-chunks arranged as cube (YXZ index order) in the aligned grid.
	//
	// All pointers must be valid, but can point to special dummy objects.
	void generateFromFinerLod(std::span<const PseudoChunkData *const, 8> finer);

	bool empty() const noexcept { return m_faces.empty(); }

	const auto &faces() const noexcept { return m_faces; }
	const auto &edgeEntries() const noexcept { return m_edge_entries; }
	const auto &cellEntries() const noexcept { return m_cell_entries; }

private:
	extras::dyn_array<Face> m_faces;
	extras::dyn_array<EdgeEntry> m_edge_entries;
	extras::dyn_array<CellEntry> m_cell_entries;
};

} // namespace voxen::land
