#pragma once

#include <voxen/common/scratch_memory_allocator.hpp>
#include <voxen/common/scratch_memory_containers.hpp>
#include <voxen/gfx/ui/ui_render_data.hpp>
#include <voxen/util/packed_color.hpp>

namespace voxen::gfx::ui::detail
{

class RenderListBuilder {
public:
	RenderListBuilder(ScratchMemoryAllocatorScope &scratch, size_t items_estimate);

	uint32_t addItem(glm::vec2 min, glm::vec2 max);
	void addRectangle(glm::vec2 min, glm::vec2 max, PackedColorSrgb color, uint32_t item_id);

	RenderData produceRenderData();

private:
	DrawSetupRenderData m_cur_draw_setup = {};

	scratch_vector<DrawSetupRenderData> m_draw_setups;
	scratch_vector<PerItemRenderData> m_per_item_datas;
	scratch_vector<VertexRenderData> m_vertices;
	scratch_vector<RenderData::IndexType> m_indices;

	void splitDraw();
};

} // namespace voxen::gfx::ui::detail
