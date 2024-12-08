#pragma once

#include <voxen/land/land_fwd.hpp>

#include <extras/dyn_array.hpp>

#include <cstdint>
#include <span>

namespace voxen::land
{

// TODO: move this struct to gfx codegen definitions
struct PseudoSurfaceVertexPosition {
	uint16_t position_x_unorm;
	uint16_t position_y_unorm;
	uint16_t position_z_unorm;
};

// TODO: move this struct to gfx codegen definitions
struct PseudoSurfaceVertexAttributes {
	int16_t normal_x_snorm;
	int16_t normal_y_snorm;
	int16_t normal_z_snorm;

	uint16_t histogram_mat_id_or_color[4];
	uint8_t histogram_mat_weight[4];
};

class PseudoChunkSurface {
public:
	PseudoChunkSurface() = default;
	PseudoChunkSurface(PseudoChunkSurface &&) = default;
	PseudoChunkSurface(const PseudoChunkSurface &) = default;
	PseudoChunkSurface &operator=(PseudoChunkSurface &&) = default;
	PseudoChunkSurface &operator=(const PseudoChunkSurface &) = default;
	~PseudoChunkSurface() = default;

	// TODO: temporary, debug stuff; remove this
	void generate(const PseudoChunkData &data);

	// Generate pseudo-chunk surface from pseudo-chunk data of the same LOD.
	// Arrangement of pointers in the array must be this:
	// - [0] - "primary" chunk that will "own" the surface
	// - [1; 7) - its 6 "face" adjacent chunks in cubemap order (X+, X-, Y+, Y-, Z+, Z-)
	// - [8; 21) - its 12 "edge" adjacent chunks in edge numbering order
	//
	// All pointers must be valid, but can point to special dummy objects.
	void generate(std::span<const PseudoChunkData *const, 21> datas);

	// Vertex array size is guaranteed to never exceed UINT32_MAX (actually even UINT16_MAX due to 16-bit index)
	uint32_t numVertices() const noexcept { return static_cast<uint32_t>(m_vertex_positions.size()); }
	const PseudoSurfaceVertexPosition *vertexPositions() const noexcept { return m_vertex_positions.data(); }
	const PseudoSurfaceVertexAttributes *vertexAttributes() const noexcept { return m_vertex_attributes.data(); }

	// Index array size is guaranteed to never exceed UINT32_MAX
	uint32_t numIndices() const noexcept { return static_cast<uint32_t>(m_indices.size()); }
	const uint16_t *indices() const noexcept { return m_indices.data(); }

	bool empty() const noexcept { return m_indices.empty(); }

private:
	extras::dyn_array<PseudoSurfaceVertexPosition> m_vertex_positions;
	extras::dyn_array<PseudoSurfaceVertexAttributes> m_vertex_attributes;
	extras::dyn_array<uint16_t> m_indices;
};

} // namespace voxen::land
