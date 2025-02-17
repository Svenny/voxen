#pragma once

#include <voxen/gfx/vk/vk_transient_buffer_allocator.hpp>
#include <voxen/util/packed_color.hpp>

#include <glm/vec2.hpp>

#include <vector>

namespace voxen::gfx::ui::detail
{

// TODO: unif with GPU code
struct PerItemData {
	glm::vec2 scissor_min;
	glm::vec2 scissor_max;
};

// TODO: unify with GPU code
struct VertexData {
	glm::vec2 position;
	glm::vec2 uv;
	PackedColorSrgb color;
	uint32_t item_id;
};

struct DrawCommand {
	uint32_t draw_count;

	VkBuffer indirect_buffer_handle;
	VkBuffer per_item_buffer_handle;
	VkBuffer vertex_buffer_handle;
	VkBuffer index_buffer_handle;

	VkDeviceSize indirect_buffer_offset;
	VkDeviceSize per_item_buffer_offset;
	VkDeviceSize vertex_buffer_offset;
	VkDeviceSize index_buffer_offset;

	VkDeviceSize per_item_buffer_byte_range;
	VkDeviceSize vertex_buffer_byte_range;
	VkDeviceSize index_buffer_byte_range;
};

class RenderListBuilder {
public:
	using IndexType = uint16_t;

	void clear() noexcept;

	uint32_t addItem(glm::vec2 min, glm::vec2 max);
	void addRectangle(glm::vec2 min, glm::vec2 max, PackedColorSrgb color, uint32_t item_id);

	DrawCommand uploadAndClear(vk::TransientBufferAllocator &tba);

private:
	struct DrawSetup {
		uint32_t vertex_offset = 0;
		uint32_t index_offset = 0;
		uint32_t vertex_count = 0;
		uint32_t index_count = 0;
	};

	DrawSetup m_cur_draw_setup;

	std::vector<DrawSetup> m_draw_setups;
	std::vector<PerItemData> m_per_item_datas;
	std::vector<VertexData> m_vertices;
	std::vector<IndexType> m_indices;

	void splitDraw();
};

} // namespace voxen::gfx::ui::detail
