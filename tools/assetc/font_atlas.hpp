#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace assetc
{

struct FontAtlasParams {
	// Input font file (anything FreeType can open)
	std::filesystem::path font_path;
	// Output single-channel SDF atlas PNG
	std::filesystem::path atlas_path;
	// Optional output of glyph metrics as C++ declarations, skipped if empty
	std::filesystem::path metrics_path;

	// Nominal glyph size in points (at 96 DPI)
	int glyph_size = 32;
	// SDF spread in pixels
	int sdf_spread = 4;

	// Print font and glyph metrics to stdout
	bool verbose = false;
};

// Render printable ASCII characters from a font to a signed distance field atlas
std::expected<void, std::string> compileFontAtlas(const FontAtlasParams &params);

} // namespace assetc
