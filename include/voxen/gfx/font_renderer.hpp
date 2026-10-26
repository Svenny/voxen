#pragma once

#include <voxen/gfx/gfx_fwd.hpp>

#include <m/color.hpp>
#include <m/vector.hpp>

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
		m::float2 up_left_pos;
		m::float2 lo_right_pos;
		m::float2 up_left_uv;
		m::float2 lo_right_uv;
		m::uint8_t4 color_srgb;
	};

	struct TextItem {
		std::string_view text;
		m::float2 origin_screen;
		m::rgba8_srgb color;
	};

	FontRenderer(GfxSystem &gfx);
	FontRenderer(FontRenderer &&) = delete;
	FontRenderer(const FontRenderer &) = delete;
	FontRenderer &operator=(FontRenderer &&) = delete;
	FontRenderer &operator=(const FontRenderer &) = delete;
	~FontRenderer();

	void loadResources();
	std::vector<GlyphCommand> getGlyphCommands(std::span<const TextItem> text_items);
	void drawUi(VkCommandBuffer cmd_buf, std::span<const TextItem> text_items, m::float2 inv_screen_size);

private:
	struct Resources;

	GfxSystem &m_gfx;

	std::unique_ptr<Resources> m_resources;
	float m_font_scaling = 1.0f;

	void createFontAtlasTexture();
};

} // namespace voxen::gfx
