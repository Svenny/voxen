#include "ui_render_list_builder.hpp"

#include <limits>

namespace voxen::gfx::ui::detail
{

void RenderListBuilder::clear() noexcept
{
	m_cur_draw_setup = {};

	m_draw_setups.clear();
	m_per_item_datas.clear();
	m_vertices.clear();
	m_indices.clear();
}

uint32_t RenderListBuilder::addItem(glm::vec2 min, glm::vec2 max)
{
	uint32_t index = static_cast<uint32_t>(m_per_item_datas.size());

	m_per_item_datas.emplace_back(PerItemData {
		.scissor_min = min,
		.scissor_max = max,
	});

	return index;
}

void RenderListBuilder::addRectangle(glm::vec2 min, glm::vec2 max, PackedColorSrgb color, uint32_t item_id)
{
	if (m_cur_draw_setup.vertex_count + 4 > std::numeric_limits<IndexType>::max()) {
		splitDraw();
	}

	VertexData vd {
		.position = {},
		.uv = {},
		.color = color,
		.item_id = item_id,
	};

	// 0-2
	// |/|  0-2-1, 3-1-2
	// 1-3

	vd.position = min;
	m_vertices.emplace_back(vd);

	vd.position = glm::vec2(min.x, max.y);
	m_vertices.emplace_back(vd);

	vd.position = glm::vec2(max.x, min.y);
	m_vertices.emplace_back(vd);

	vd.position = max;
	m_vertices.emplace_back(vd);

	const IndexType base = static_cast<IndexType>(m_cur_draw_setup.vertex_count);
	m_indices.insert(m_indices.end(), { IndexType(base + 0), IndexType(base + 2), IndexType(base + 1) });
	m_indices.insert(m_indices.end(), { IndexType(base + 3), IndexType(base + 1), IndexType(base + 2) });

	m_cur_draw_setup.vertex_count += 4;
	m_cur_draw_setup.index_count += 6;
}

DrawCommand RenderListBuilder::uploadAndClear(vk::TransientBufferAllocator &tba)
{
	// Add the last draw setup to the list
	splitDraw();

	DrawCommand draw_cmd;
	draw_cmd.draw_count = static_cast<uint32_t>(m_draw_setups.size());

	// Upload indirect command data
	{
		const size_t indirect_data_size = sizeof(VkDrawIndexedIndirectCommand) * m_draw_setups.size();
		auto indirect_buffer_alloc = tba.allocate(vk::TransientBufferAllocator::TypeUpload, indirect_data_size,
			alignof(VkDrawIndexedIndirectCommand));

		auto *write_indirect = reinterpret_cast<VkDrawIndexedIndirectCommand *>(indirect_buffer_alloc.host_pointer);

		for (size_t i = 0; i < m_draw_setups.size(); i++) {
			write_indirect[i] = {
				.indexCount = m_draw_setups[i].index_count,
				.instanceCount = 1,
				.firstIndex = m_draw_setups[i].index_offset,
				.vertexOffset = static_cast<int32_t>(m_draw_setups[i].vertex_offset),
				.firstInstance = 0,
			};
		}

		draw_cmd.indirect_buffer_handle = indirect_buffer_alloc.buffer;
		draw_cmd.indirect_buffer_offset = indirect_buffer_alloc.buffer_offset;
	}

	// Upload per-item data
	{
		const size_t per_item_data_size = sizeof(PerItemData) * m_per_item_datas.size();
		auto per_item_buffer_alloc = tba.allocate(vk::TransientBufferAllocator::TypeUpload, per_item_data_size,
			64); // TODO: max(alignof(PerItemData), vk::minSsboOffsetAlign);
		memcpy(per_item_buffer_alloc.host_pointer, m_per_item_datas.data(), per_item_data_size);

		draw_cmd.per_item_buffer_handle = per_item_buffer_alloc.buffer;
		draw_cmd.per_item_buffer_offset = per_item_buffer_alloc.buffer_offset;
		draw_cmd.per_item_buffer_byte_range = per_item_data_size;
	}

	// Upload vertices
	{
		const size_t vertex_data_size = sizeof(VertexData) * m_vertices.size();
		auto vertex_buffer_alloc = tba.allocate(vk::TransientBufferAllocator::TypeUpload, vertex_data_size,
			64); // TODO: max(alignof(VertexData), vk::minSsboOffsetAlign);
		memcpy(vertex_buffer_alloc.host_pointer, m_vertices.data(), vertex_data_size);

		draw_cmd.vertex_buffer_handle = vertex_buffer_alloc.buffer;
		draw_cmd.vertex_buffer_offset = vertex_buffer_alloc.buffer_offset;
		draw_cmd.vertex_buffer_byte_range = vertex_data_size;
	}

	// Upload indices
	{
		const size_t index_data_size = sizeof(IndexType) * m_indices.size();
		auto index_buffer_alloc = tba.allocate(vk::TransientBufferAllocator::TypeUpload, index_data_size,
			alignof(IndexType));
		memcpy(index_buffer_alloc.host_pointer, m_indices.data(), index_data_size);

		draw_cmd.index_buffer_handle = index_buffer_alloc.buffer;
		draw_cmd.index_buffer_offset = index_buffer_alloc.buffer_offset;
		draw_cmd.index_buffer_byte_range = index_data_size;
	}

	// TODO: `shrink_to_fit` if we have too much free capacity?
	clear();

	return draw_cmd;
}

void RenderListBuilder::splitDraw()
{
	// Only add to draw list if there is something to draw
	if (m_cur_draw_setup.index_count > 0) {
		m_draw_setups.emplace_back(m_cur_draw_setup);
	}

	m_cur_draw_setup = {};
	m_cur_draw_setup.vertex_offset = static_cast<uint32_t>(m_vertices.size());
	m_cur_draw_setup.index_offset = static_cast<uint32_t>(m_indices.size());
}

} // namespace voxen::gfx::ui::detail
