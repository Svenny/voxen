#pragma once

#include <voxen/util/packed_color.hpp>

#include <glm/vec2.hpp>

#include <span>

namespace voxen::gfx::ui
{

// TODO: unify with GPU code
struct PerItemRenderData {
	glm::vec2 scissor_min;
	glm::vec2 scissor_max;
};

// TODO: unify with GPU code
struct VertexRenderData {
	glm::vec2 position;
	glm::vec2 uv;
	PackedColorSrgb color;
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
