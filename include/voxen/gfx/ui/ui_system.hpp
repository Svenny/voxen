#pragma once

#include <voxen/common/common_fwd.hpp>
#include <voxen/gfx/ui/ui_fwd.hpp>
#include <voxen/util/resolution.hpp>

#include <extras/pimpl.hpp>

namespace voxen::gfx::ui
{

// Immediate-mode UI system inspired by two great libraries:
// - Dear ImGui (https://github.com/ocornut/imgui)
// - Clay (https://github.com/nicbarker/clay)
//
// Performs everything (layout, input routing, text shaping,
// render data preparation etc.) except the actual rendering,
// which is up to render graphs.
//
// Intended to be used for all 2D cases - tooling/editor GUI,
// in-game GUI (menus), gameplay HUD and debug interfaces.
// Not suitable for 3D (in-scene) UI elements.
//
// This class, as well as everything in this namespace, is NOT thread-safe.
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

	// Begin a new frame.
	//
	// Note that frame sequencing is very important for proper input handling.
	// E.g. when detecting "hovered" state (see `ScopedContainer`) item positions
	// computed in previous frame are used. This means if you insert an empty pair
	// of `beginFrame()/endFrame()` between actual frames then all "history" of
	// items' positions will be lost, making any input handling impossible.
	//
	// `scratch` scope must NOT close until `endFrame()` call.
	// Note that calls to `UiBuilder` also allocate from scratch memory,
	// so be very careful when using subscopes around UI building code.
	//
	// Returned UI builder can be used until matching `endFrame()` call.
	UiBuilder beginFrame(ScratchMemoryAllocatorScope &scratch, PerFrameData per_frame);

	// Calculate layout of UI elements and prepare render data.
	// Must have a matching prior `beginFrame()` call.
	//
	// Pointers (spans) inside the returned structure are valid until `scratch` scope
	// passed to `beginFrame()` closes. Make sure to upload data to GPU before that.
	RenderData endFrame();

private:
	extras::pimpl<detail::UiSystemImpl, 160, 8> m_impl;
};

} // namespace voxen::gfx::ui
