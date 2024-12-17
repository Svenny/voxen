#pragma once

#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/util/packed_color.hpp>

#include <glm/vec2.hpp>

#include <memory>
#include <span>
#include <string_view>
#include <vector>

typedef struct VkCommandBuffer_T *VkCommandBuffer;

namespace voxen::gfx
{

class FontRenderer {
public:
	struct GlyphCommand {
		glm::vec2 up_left_pos;
		glm::vec2 lo_right_pos;
		glm::vec2 up_left_uv;
		glm::vec2 lo_right_uv;
		glm::u8vec4 color_srgb;
	};

	struct TextItem {
		std::string_view text;
		glm::vec2 origin_screen;
		PackedColorSrgb color;
	};

	FontRenderer(GfxSystem &gfx);
	FontRenderer(FontRenderer &&) = delete;
	FontRenderer(const FontRenderer &) = delete;
	FontRenderer &operator=(FontRenderer &&) = delete;
	FontRenderer &operator=(const FontRenderer &) = delete;
	~FontRenderer();

	void loadResources();
	std::vector<GlyphCommand> getGlyphCommands(std::span<const TextItem> text_items);
	void drawUi(VkCommandBuffer cmd_buf, std::span<const TextItem> text_items, glm::vec2 inv_screen_size);

private:
	struct Resources;

	GfxSystem &m_gfx;

	std::unique_ptr<Resources> m_resources;
	float m_font_scaling = 1.0f;

	void createFontAtlasTexture();
};

} // namespace voxen::gfx
