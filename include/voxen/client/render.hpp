#pragma once

#include <voxen/client/window.hpp>

#include <voxen/common/world.hpp>
#include <voxen/common/gameview.hpp>

namespace voxen::client
{

class Render {
public:
	explicit Render(Window &window);
	Render(Render &&) = delete;
	Render(const Render &) = delete;
	Render &operator = (Render &&) = delete;
	Render &operator = (const Render &) = delete;
	~Render();

	void drawFrame(const World &state, const GameView &view);
private:
	Window &m_window;
};

}
