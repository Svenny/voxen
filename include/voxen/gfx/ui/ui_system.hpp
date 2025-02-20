#pragma once

#include <voxen/gfx/ui/ui_fwd.hpp>
#include <voxen/util/resolution.hpp>

#include <extras/pimpl.hpp>

namespace voxen::gfx::ui
{

class UiSystem {
public:
	struct PerFrameData {
		Resolution window_resolution;
		glm::dvec2 cursor_position;
		bool left_button_pressed;
	};

	UiSystem();
	UiSystem(UiSystem &&) = delete;
	UiSystem(const UiSystem &) = delete;
	UiSystem &operator=(UiSystem &&) = delete;
	UiSystem &operator=(const UiSystem &) = delete;
	~UiSystem();

	UiBuilder beginFrame(PerFrameData per_frame);
	RenderData endFrame();

private:
	extras::pimpl<detail::UiSystemImpl, 400, 8> m_impl;
};

} // namespace voxen::gfx::ui
