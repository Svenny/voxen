#pragma once

#include <voxen/client/gameview.hpp>
#include <voxen/os/os_fwd.hpp>
#include <voxen/os/window_events.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/visibility.hpp>
#include <voxen/world/world_fwd.hpp>

namespace voxen::client
{

class VOXEN_API Gui {
public:
	explicit Gui(os::Window& window);
	~Gui() noexcept;

	// Feed an input event received from the window
	void handleEvent(const os::WindowEvent& event);

	void init(const world::State& world_start_state);
	void update(const world::State& lastState, double dt, svc::MessageSender& msend);

	GameView& view();

private:
	GameView m_gameview;
	// TODO stack for another GUI elements
};

} // namespace voxen::client
