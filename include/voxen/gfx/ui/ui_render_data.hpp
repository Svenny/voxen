#pragma once

#include <m/color.hpp>
#include <m/vector.hpp>

#include <cstdint>
#include <span>

namespace voxen::gfx::ui
{

// TODO: unify with GPU code
struct PerItemRenderData {
	m::float2 scissor_min;
	m::float2 scissor_max;
};

// TODO: unify with GPU code
struct VertexRenderData {
	m::float2 position;
	m::float2 uv;
	m::rgba8_srgb color;
	uint32_t item_id;
};

struct DrawSetupRenderData {
	uint32_t first_vertex;
	uint32_t vertex_count;

	uint32_t first_index;
	uint32_t index_count;
};

struct RenderData {
	using IndexType = uint16_t;

	std::span<const PerItemRenderData> per_item_buffer;
	std::span<const VertexRenderData> vertex_buffer;
	std::span<const IndexType> index_buffer;
	std::span<const DrawSetupRenderData> draw_setups;
};

} // namespace voxen::gfx::ui
