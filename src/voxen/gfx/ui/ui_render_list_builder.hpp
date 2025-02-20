#pragma once

#include <voxen/gfx/ui/ui_render_data.hpp>
#include <voxen/util/packed_color.hpp>

#include <glm/vec2.hpp>

#include <vector>

namespace voxen::gfx::ui::detail
{

class RenderListBuilder {
public:
	void clear() noexcept;

	uint32_t addItem(glm::vec2 min, glm::vec2 max);
	void addRectangle(glm::vec2 min, glm::vec2 max, PackedColorSrgb color, uint32_t item_id);

	RenderData produceRenderData();

private:
	DrawSetupRenderData m_cur_draw_setup;

	std::vector<DrawSetupRenderData> m_draw_setups;
	std::vector<PerItemRenderData> m_per_item_datas;
	std::vector<VertexRenderData> m_vertices;
	std::vector<RenderData::IndexType> m_indices;

	void splitDraw();
};

} // namespace voxen::gfx::ui::detail
