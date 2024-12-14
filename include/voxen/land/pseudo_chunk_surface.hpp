#pragma once

#include <voxen/land/land_fwd.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace voxen::land
{

// TODO: move this struct to gfx codegen definitions
struct PseudoSurfaceVertexPosition {
	// Vertex position in "extended" chunk-local space to allow
	// going slightly out of chunk bounds for stitching/skirting.
	// After UNORM unpacking expand it into [-0.125:1.125] range.
	glm::u16vec3 position_unorm;
};

// TODO: move this struct to gfx codegen definitions
struct PseudoSurfaceVertexAttributes {
	// Normal compressed using octahedral method, then packed into two 16-bit SNORM values.
	// Octahedral method is taken from paper "A Survey of Efficient Representations for Independent Unit Vectors":
	// http://jcgt.org/published/0003/02/01/paper.pdf
	// The only difference is that Y and Z axes are swapped in this implementation.
	glm::i16vec2 normal_oct_snorm;
	// Material histogram (MatHist) stores up to 4 materials with the "highest presence"
	// in this vertex. This attribute is guaranteed to be the same for all three vertices
	// comprising a triangle.
	// An entry is a 16-bit value which can be treated as either material ID if its highest
	// bit is zero ([0:2^15) range) or as fixed R5G5B5 color otherwise ([2^15:2^16) range).
	// Material ID assignment and encoding/decoding of colors is the responsibility
	// of block registry system, and is not covered here.
	glm::u16vec4 mat_hist_entries;
	// Blending weights of `mat_hist_entries` elements, full range [0; 255].
	// Zero weight means the entry is ignored, it can then contain garbage data.
	//
	// While the exact blending process is up to the renderer, these values are
	// calculated relative to each other - basically they define a weighted sum.
	glm::u8vec4 mat_hist_weights;
};

class PseudoChunkSurface {
public:
	PseudoChunkSurface() = default;
	PseudoChunkSurface(PseudoChunkSurface &&) = default;
	PseudoChunkSurface(const PseudoChunkSurface &) = default;
	PseudoChunkSurface &operator=(PseudoChunkSurface &&) = default;
	PseudoChunkSurface &operator=(const PseudoChunkSurface &) = default;
	~PseudoChunkSurface() = default;

	// Generate from chunk+adjacency references.
	// XXX: this is temporary solution until a separate "true geometry" class is added.
	void generate(ChunkAdjacencyRef adj);

	// Generate pseudo-chunk surface from pseudo-chunk data of the same LOD.
	// Arrangement of pointers in the array must be this:
	// - [0] - "primary" chunk that will "own" the surface
	// - [1:7) - its 6 face-adjacent chunks in cubemap order (X+, X-, Y+, Y-, Z+, Z-)
	// - [7:11) - edge-adjacent chunks for X edge (YZ order)
	// - [11:15) - edge-adjacent chunks for Y edge (XZ order)
	// - [15:19) - edge-adjacent chunks for Z edge (YX order)
	//
	// All pointers must be valid, but can point to special dummy objects.
	//
	// `lod` parameter drives "artistic" fixups.
	void generate(std::span<const PseudoChunkData *const, 19> datas, uint32_t lod);

	// Vertex array size is guaranteed to never exceed UINT32_MAX (actually even UINT16_MAX due to 16-bit index)
	uint32_t numVertices() const noexcept { return static_cast<uint32_t>(m_vertex_positions.size()); }
	const PseudoSurfaceVertexPosition *vertexPositions() const noexcept { return m_vertex_positions.data(); }
	const PseudoSurfaceVertexAttributes *vertexAttributes() const noexcept { return m_vertex_attributes.data(); }

	// Index array size is guaranteed to never exceed UINT32_MAX
	uint32_t numIndices() const noexcept { return static_cast<uint32_t>(m_indices.size()); }
	const uint16_t *indices() const noexcept { return m_indices.data(); }

	bool empty() const noexcept { return m_indices.empty(); }

private:
	std::vector<PseudoSurfaceVertexPosition> m_vertex_positions;
	std::vector<PseudoSurfaceVertexAttributes> m_vertex_attributes;
	std::vector<uint16_t> m_indices;
};

} // namespace voxen::land
