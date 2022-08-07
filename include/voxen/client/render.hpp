#pragma once

#include <voxen/client/window.hpp>

#include <voxen/common/world_state.hpp>
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

	void drawFrame(const WorldState &world_state, const GameView &view);
private:
	Window &m_window;
};

}
