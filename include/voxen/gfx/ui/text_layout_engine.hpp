#pragma once

#include <extras/pimpl.hpp>

#include <string_view>

namespace voxen::gfx::ui
{

enum class FixedFontType : uint8_t {
	SansSerif,
	Serif,
	Monospace,
};

struct FixedFont {
	FixedFontType type = FixedFontType::SansSerif;
	bool italic = false;
	bool bold = false;
	uint8_t size = 16;
};

class TextLayoutEngine {
public:
	void layoutText(std::u32string_view text, FixedFont font);

private:
};

} // namespace voxen::gfx::ui
