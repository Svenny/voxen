#pragma once

#include <voxen/land/chunk_key.hpp>
#include <voxen/land/land_fwd.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vector>

namespace voxen::land
{

class PseudoChunkData {
public:
	struct CellEntry {
		// Cell index (x; y; z), coordinates are in range [0; Consts::CHUNK_SIZE_BLOCKS)
		glm::u8vec3 cell_index;
		// Mask of "is block solid" bits (1 - solid, 0 - empty) for cell corners, YXZ order.
		// There are 12 pairs of bits corresponding to 12 cube edges - these pairs will be
		// examined when generating geometry to evaluate where the surface is crossed.
		uint8_t corner_solid_mask;

		// Material histogram (MatHist) stores up to 4 materials with the "highest presence"
		// in this cell. An entry is a 16-bit value which can be treated as either material
		// ID if its highest bit is zero ([0:2^15) range) or as fixed R5G5B5 color otherwise
		// ([2^15:2^16) range). Material ID assignment and encoding/decoding of colors is
		// the responsibility of block registry system, and is not covered here.
		glm::u16vec4 mat_hist_entries;
		// Blending weights of `mat_hist_entries` elements, full range [0; 255].
		// Zero weight means the entry is ignored, it can then contain garbage data.
		//
		// While the exact blending process is up to the renderer, these values are
		// calculated relative to each other - basically they define a weighted sum.
		glm::u8vec4 mat_hist_weights;

		// "Representative point" of this cell in chunk-local space stored
		// as 16-bit UNORM values in [0..1] range. This represents an average
		// of multiple surface points from finer resolution chunks and will
		// be used to place vertex for this cell when generating geometry.
		glm::u16vec3 surface_point_unorm;
		// Number of finer-resolution surface points contributed to `surface_point_unorm`
		// calculation, clamped to UINT16_MAX. Acts as a weighting factor - during next
		// aggregations points with larger counts will "drag" the average towards them.
		uint16_t surface_point_sum_count;
	};

	using CellEntryArray = std::vector<CellEntry>;

	PseudoChunkData(ChunkKey ck) noexcept : m_output_key(ck) {}

	// Generate pseudo-chunk LOD1 data from 27 (8 + 3x4 + 3x2 + 1) LOD0 (true) chunks.
	// Arrangement of pointers in the array must be this:
	// - [0:8) - "primary" LOD0 chunks comprising the LOD1 cube in YXZ index order
	// - [8:12) - face-adjacent chunks from X+ direction (east of "primary" ones) in YZ index order
	// - [12:16) - face-adjacent chunks from Y+ direction (above "primary" ones) in XZ index order
	// - [16:20) - face-adjacent chunks from Z+ direction in YX index order
	// - [20:22) - edge-adjacent chunks for X edge (+YZ, lower, higher)
	// - [22:24) - edge-adjacent chunks for Y edge (+XZ, lower, higher)
	// - [24:26) - edge-adjacent chunks for Z edge (+XY, lower, higher)
	// - 26 - vertex-adjacent chunk (XYZ direction)
	//
	// All pointers must be valid, but can point to special dummy chunks.
	void generateFromLod0(std::span<const Chunk *const, 27> chunks);
	// Generate (aggregate) pseudo-chunk data for LOD(n) from 8 LOD(n-1)
	// pseudo-chunks arranged as cube (YXZ index order) in the aligned grid.
	//
	// All pointers must be valid, but can point to special dummy objects.
	void generateFromFinerLod(std::span<const PseudoChunkData *const, 8> finer);
	// TODO: describe me
	void generateExternally(std::span<const CellEntry> cells);

	// Find `CellEntry` with `cell_index` by binary search.
	// Returns pointer to this entry or null if it was not found.
	const CellEntry *findEntry(glm::uvec3 cell_index) const noexcept;

	bool empty() const noexcept { return m_cell_entries.empty(); }

	// Array is sorted by `CellEntry::cell_index` in (y, x, z) tuple comparison order
	const CellEntryArray &cellEntries() const noexcept { return m_cell_entries; }

private:
	CellEntryArray m_cell_entries;
	// TODO: only for debugging
	ChunkKey m_output_key;
};

} // namespace voxen::land
