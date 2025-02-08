#pragma once

#include <voxen/gfx/ui/ui_builder.hpp>

#include <string>

namespace vxgame
{

class Ui {
public:
	Ui();

	void draw(voxen::gfx::ui::UiBuilder &ui);

private:
	std::u8string m_version_string;
};

} // namespace vxgame
