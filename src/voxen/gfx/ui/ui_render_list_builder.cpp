#include "ui_render_list_builder.hpp"

#include <limits>

namespace voxen::gfx::ui::detail
{

RenderListBuilder::RenderListBuilder(ScratchMemoryAllocatorScope &scratch, size_t items_estimate)
	: m_draw_setups(scratch), m_per_item_datas(scratch), m_vertices(scratch), m_indices(scratch)
{
	m_draw_setups.reserve(1);
	m_per_item_datas.reserve(items_estimate);
	m_vertices.reserve(4 * items_estimate);
	m_indices.reserve(6 * items_estimate);
}

uint32_t RenderListBuilder::addItem(glm::vec2 min, glm::vec2 max)
{
	uint32_t index = static_cast<uint32_t>(m_per_item_datas.size());

	m_per_item_datas.emplace_back(PerItemRenderData {
		.scissor_min = min,
		.scissor_max = max,
	});

	return index;
}

void RenderListBuilder::addRectangle(glm::vec2 min, glm::vec2 max, PackedColorSrgb color, uint32_t item_id)
{
	using IDT = RenderData::IndexType;

	if (m_cur_draw_setup.vertex_count + 4 > std::numeric_limits<IDT>::max()) {
		splitDraw();
	}

	VertexRenderData vd {
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

	const IDT base = static_cast<IDT>(m_cur_draw_setup.vertex_count);
	m_indices.insert(m_indices.end(), { IDT(base + 0), IDT(base + 2), IDT(base + 1) });
	m_indices.insert(m_indices.end(), { IDT(base + 3), IDT(base + 1), IDT(base + 2) });

	m_cur_draw_setup.vertex_count += 4;
	m_cur_draw_setup.index_count += 6;
}

RenderData RenderListBuilder::produceRenderData()
{
	// Add the last draw setup to the list
	splitDraw();

	return {
		.per_item_buffer = m_per_item_datas,
		.vertex_buffer = m_vertices,
		.index_buffer = m_indices,
		.draw_setups = m_draw_setups,
	};
}

void RenderListBuilder::splitDraw()
{
	// Only add to draw list if there is something to draw
	if (m_cur_draw_setup.index_count > 0) {
		m_draw_setups.emplace_back(m_cur_draw_setup);
	}

	m_cur_draw_setup = {};
	m_cur_draw_setup.first_vertex = static_cast<uint32_t>(m_vertices.size());
	m_cur_draw_setup.first_index = static_cast<uint32_t>(m_indices.size());
}

} // namespace voxen::gfx::ui::detail
