#pragma once

// TODO: temporary debug stuff
#include <voxen/land/pseudo_chunk_data.hpp>

#include <extras/dyn_array.hpp>

#include <array>
#include <cstdint>
#include <span>

namespace voxen::land
{

// TODO: move this struct to gfx codegen definitions
struct PseudoSurfaceVertex {
	uint16_t position_x_unorm;
	uint16_t position_y_unorm;

	uint16_t position_z_unorm;
	int16_t normal_x_snorm;

	int16_t normal_y_snorm;
	int16_t normal_z_snorm;

	uint16_t histogram_mat_or_color[4];
	uint8_t histogram_mat_weight[4];
};

class PseudoChunkSurface {
public:
	PseudoChunkSurface() = default;
	// TODO: temporary debug stuff
	explicit PseudoChunkSurface(const PseudoChunkData &data) : m_temp_debug_pseudo_data(data) {}
	explicit PseudoChunkSurface(std::span<const PseudoSurfaceVertex> vertices, std::span<const uint32_t> indices);
	PseudoChunkSurface(PseudoChunkSurface &&) = default;
	PseudoChunkSurface(const PseudoChunkSurface &) = default;
	PseudoChunkSurface &operator=(PseudoChunkSurface &&) = default;
	PseudoChunkSurface &operator=(const PseudoChunkSurface &) = default;
	~PseudoChunkSurface() = default;

	const PseudoChunkData &tempDebugPseudoData() const noexcept { return m_temp_debug_pseudo_data; }

	// Vertex array size is guaranteed to never exceed UINT32_MAX (actually even UINT16_MAX due to 16-bit index)
	uint32_t numVertices() const noexcept { return static_cast<uint32_t>(m_vertices.size()); }
	const PseudoSurfaceVertex *vertices() const noexcept { return m_vertices.data(); }

	// Index array size is guaranteed to never exceed UINT32_MAX
	uint32_t numIndices() const noexcept { return static_cast<uint32_t>(m_indices.size()); }
	const uint16_t *indices() const noexcept { return m_indices.data(); }

	bool empty() const noexcept { return m_indices.empty(); }

	// Build surface from pseudo-chunk and its adjacent chunks.
	// This operation takes considerable time. Don't call it on the main thread.
	static PseudoChunkSurface build(const PseudoChunkData &data, std::array<const PseudoChunkData *, 6> adjacent);

private:
	// TODO: temporary debug stuff
	PseudoChunkData m_temp_debug_pseudo_data;
	extras::dyn_array<PseudoSurfaceVertex> m_vertices;
	extras::dyn_array<uint16_t> m_indices;
};

} // namespace voxen::land
