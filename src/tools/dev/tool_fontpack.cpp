#include <voxen/common/assets/png_tools.hpp>
#include <voxen/common/filemanager.hpp>

#include <extras/defer.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H

#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

int main(int argc, char *argv[])
{
	if (argc != 4) {
		printf("Usage: %s <input/font.otf> <output/atlas.png> <output/header.json>\n", argv[0]);
		return 1;
	}

	const char *in_font_path = argv[1];
	const char *out_atlas_path = argv[2];
	const char *out_header_path = argv[3];

	FT_Library ft_library;
	FT_Error error = FT_Init_FreeType(&ft_library);
	if (error != FT_Err_Ok) {
		printf("FreeType init failed: %s\n", FT_Error_String(error));
		return 1;
	}

	defer { FT_Done_FreeType(ft_library); };

	FT_Face ft_face;
	error = FT_New_Face(ft_library, in_font_path, 0, &ft_face);
	if (error != FT_Err_Ok) {
		printf("Font open failed: %s\n", FT_Error_String(error));
		return 1;
	}

	defer { FT_Done_Face(ft_face); };

	// Note: rendered glyph sizes will be all over the place, this is only a "baseline" value
	constexpr int GLYPH_SIZE = 32;
	constexpr int RENDER_DPI = 96;
	constexpr int SDF_SPREAD = 4;

	error = FT_Set_Char_Size(ft_face, 0, GLYPH_SIZE * 64, 0, RENDER_DPI);
	if (error != FT_Err_Ok) {
		printf("Setting char size failed: %s\n", FT_Error_String(error));
		return 1;
	}

	error = FT_Property_Set(ft_library, "sdf", "spread", &SDF_SPREAD);
	if (error != FT_Err_Ok) {
		printf("Setting SDF spread failed: %s\n", FT_Error_String(error));
		return 1;
	}

	constexpr char RENDER_CHAR_MIN = ' ';
	constexpr char RENDER_CHAR_MAX = '~';

	struct GlyphInfo {
		uint32_t bitmap_width;
		uint32_t bitmap_height;
		std::vector<std::byte> bitmap;

		float bearing_x;
		float bearing_y;
		float advance_x;
	};

	uint32_t max_width = 0;
	uint32_t max_height = 0;
	std::unordered_map<char, GlyphInfo> glyph_map;

	std::stringstream description;

	{
		printf("Font metrics:\n");

		float ascent = float(ft_face->ascender) / 64.0f;
		float descent = float(ft_face->descender) / 64.0f;
		float height = float(ft_face->height) / 64.0f;
		printf("ascent = %f, descent = %f, height = %f\n\n", ascent, descent, height);

		description << "constexpr float FONT_ASCENT = " << ascent << "f;" << std::endl;
		description << "constexpr float FONT_DESCENT = " << descent << "f;" << std::endl;
		description << "constexpr float FONT_LINEHEIGHT = " << height << "f;" << std::endl;
		description << std::endl;
	}

	description << "constexpr char MIN_RENDERABLE_CHAR = '" << RENDER_CHAR_MIN << "';" << std::endl;
	description << "constexpr char MAX_RENDERABLE_CHAR = '" << RENDER_CHAR_MAX << "';" << std::endl;
	description << std::endl;

	for (char c = RENDER_CHAR_MIN; c <= RENDER_CHAR_MAX; c++) {
		error = FT_Load_Char(ft_face, FT_ULong(c), FT_LOAD_DEFAULT);
		if (error != FT_Err_Ok) {
			printf("FT_Load_Char failed for '%c': %s\n", c, FT_Error_String(error));
			return 1;
		}

		error = FT_Render_Glyph(ft_face->glyph, FT_RENDER_MODE_SDF);
		if (error != FT_Err_Ok) {
			printf("FT_Render_Glyph failed: %s\n", FT_Error_String(error));
			return 1;
		}

		FT_Bitmap *bitmap = &ft_face->glyph->bitmap;
		if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY) {
			printf("Unexpected FreeType bitmap pixel format %d\n", bitmap->pixel_mode);
			return 1;
		}

		max_width = std::max(max_width, bitmap->width);
		max_height = std::max(max_height, bitmap->rows);

		auto &map_entry = glyph_map[c];
		map_entry.bitmap_width = bitmap->width;
		map_entry.bitmap_height = bitmap->rows;

		map_entry.bearing_x = float(ft_face->glyph->bitmap_left);
		map_entry.bearing_y = float(ft_face->glyph->bitmap_top);
		map_entry.advance_x = float(ft_face->glyph->metrics.horiAdvance) / 64.0f;

		const std::byte *data_begin = reinterpret_cast<const std::byte *>(bitmap->buffer);
		const std::byte *data_end = data_begin + bitmap->width * bitmap->rows;
		map_entry.bitmap = std::vector<std::byte>(data_begin, data_end);
	}

	printf("Max glyph dimensions: %ux%u\n", max_width, max_height);

	constexpr int NUM_GLYPHS = RENDER_CHAR_MAX + 1 - RENDER_CHAR_MIN;
	constexpr int GLYPHS_PER_ROW = 12;
	constexpr int GLYPH_ROWS = (NUM_GLYPHS + GLYPHS_PER_ROW - 1) / GLYPHS_PER_ROW;

	uint32_t pixmap_width = GLYPHS_PER_ROW * max_width;
	uint32_t pixmap_height = GLYPH_ROWS * max_height;

	printf("Pixmap size: %ux%u\n", pixmap_width, pixmap_height);
	std::vector<std::byte> combined_pixmap(pixmap_width * pixmap_height);

	description << "constexpr int32_t FONT_ATLAS_WIDTH = " << pixmap_width << ';' << std::endl;
	description << "constexpr int32_t FONT_ATLAS_HEIGHT = " << pixmap_height << ';' << std::endl;
	description << std::endl;

	description << "constexpr struct GlyphInfo {" << std::endl;
	description << "\tchar c;" << std::endl;
	description << "\tfloat atlas_x;" << std::endl;
	description << "\tfloat atlas_y;" << std::endl;
	description << "\tfloat width;" << std::endl;
	description << "\tfloat height;" << std::endl;
	description << "\tfloat bearing_x;" << std::endl;
	description << "\tfloat bearing_y;" << std::endl;
	description << "\tfloat advance_x;" << std::endl;
	description << "} GLYPH_INFOS[] = {" << std::endl;

	printf("\nGlyph metrics:\n");

	for (char c = RENDER_CHAR_MIN; c <= RENDER_CHAR_MAX; c++) {
		const auto &map_entry = glyph_map[c];

		uint32_t first_out_row = max_height * uint32_t((c - RENDER_CHAR_MIN) / GLYPHS_PER_ROW);
		uint32_t first_out_col = max_width * uint32_t((c - RENDER_CHAR_MIN) % GLYPHS_PER_ROW);

		// Add padding to center the glyph in its slot
		first_out_row += (max_height - map_entry.bitmap_height) / 2;
		first_out_col += (max_width - map_entry.bitmap_width) / 2;

		printf(
			"c = %d, cc = %c, x = %u, y = %u, width = %u, height = %u, bearing_x = %f, bearing_y = %f, advance_x = %f\n",
			int(c), c, first_out_col, first_out_row, map_entry.bitmap_width, map_entry.bitmap_height, map_entry.bearing_x,
			map_entry.bearing_y, map_entry.advance_x);

		description << "\t{ '";
		if (c != '\\' && c != '\'') {
			description << c;
		} else {
			description << '\\' << c;
		}

		description << "', " << first_out_col << ", " << first_out_row << ", ";
		description << map_entry.bitmap_width << ", " << map_entry.bitmap_height << ", ";
		description << map_entry.bearing_x << ", " << map_entry.bearing_y << ", " << map_entry.advance_x << " },";
		description << std::endl;

		for (uint32_t r = 0; r < map_entry.bitmap_height; r++) {
			std::byte *out_scanline = combined_pixmap.data() + (first_out_row + r) * pixmap_width + first_out_col;
			const auto *in_scanline = map_entry.bitmap.data() + r * map_entry.bitmap_width;
			memcpy(out_scanline, in_scanline, map_entry.bitmap_width);
		}
	}

	description << "};" << std::endl;

	auto packed_png = voxen::assets::PngTools::pack(combined_pixmap,
		voxen::assets::PngInfo {
			.resolution = { int32_t(pixmap_width), int32_t(pixmap_height) },
			.is_16bpc = false,
			.channels = 1,
		},
		false);

	bool result = voxen::FileManager::writeUserFile(out_atlas_path, packed_png, true);
	if (!result) {
		printf("Write file failed!\n");
		return 1;
	}

	std::string descr = description.str();
	std::span<const char> descr_span = descr;

	result = voxen::FileManager::writeUserFile(out_header_path, std::as_bytes(descr_span), true);
	if (!result) {
		printf("Write description file failed!\n");
		return 1;
	}

	return 0;
}
