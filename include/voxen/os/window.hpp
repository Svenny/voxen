#pragma once

#include <voxen/os/window_events.hpp>
#include <voxen/visibility.hpp>

#include <m/vector.hpp>

#include <cstdint>
#include <span>
#include <vector>

struct SDL_Window;
union SDL_Event;

// Vulkan handles exactly as `vulkan_core.h` declares them on 64-bit platforms, avoid including it here
using VkInstance = struct VkInstance_T *;
using VkPhysicalDevice = struct VkPhysicalDevice_T *;
using VkSurfaceKHR = struct VkSurfaceKHR_T *;

namespace voxen::os
{

// OS window, the source of input events. Its methods can be called only from the main thread.
// Only one window is supported, `pollEvents()` drops events of other windows.
class VOXEN_API Window {
public:
	struct Config {
		// Size in logical units, scaled by the display content scale where the OS doesn't do it
		int width = 0;
		int height = 0;
		const char *title = nullptr;
		// Borderless fullscreen on the primary display, ignores `width` and `height`
		bool fullscreen = false;
	};

	Window() = default;
	explicit Window(Config cfg);
	Window(Window &&other) noexcept;
	Window(const Window &) = delete;
	Window &operator=(Window &&other) noexcept;
	Window &operator=(const Window &) = delete;
	~Window() noexcept;

	bool shouldClose() const;
	// Process pending OS events, `events()` then returns what has arrived since the previous call
	void pollEvents();
	// Events collected by the latest `pollEvents()`, valid until the next call to it
	std::span<const WindowEvent> events() const noexcept { return m_events; }

	// Block until OS events arrive and process them, the next `pollEvents()` returns them
	void waitEvents();

	// Window size in logical units. May differ from `framebufferSize()`
	// when the OS scales windows (e.g. Wayland with fractional scaling).
	// Minimized windows keep their last size.
	m::int2 windowSize() const;
	// Window framebuffer size in pixels. Usually you need this function.
	// Minimized windows keep their last size.
	m::int2 framebufferSize() const;

	// Current cursor position in framebuffer pixels
	m::double2 cursorPos() const;
	// Current state of a mouse button
	bool mouseButtonPressed(MouseButton button) const;

	// Show the cursor and let it move freely
	void useRegularCursor();
	// Hide and confine the cursor, report only relative motion (for mouse look)
	void useGrabbedCursor();

	// Start delivering `TextInputEvent`s (and IME composition), e.g. while a text field has focus.
	// Keep it disabled otherwise, an active IME may consume key presses meant as controls.
	void startTextInput();
	void stopTextInput();

	// Create a Vulkan surface for this window, logs the reason on failure. Its instance
	// must be created with extensions from `requiredVulkanInstanceExtensions()`.
	bool createVulkanSurface(VkInstance instance, VkSurfaceKHR *surface) const;

	// Vulkan instance extensions needed to create window surfaces, always includes `VK_KHR_surface`
	static std::span<const char *const> requiredVulkanInstanceExtensions();
	// Check if a queue family of a physical device can present to windows
	static bool vulkanPresentationSupport(VkInstance instance, VkPhysicalDevice physical_device, uint32_t queue_family);

	// Initialize the windowing library. Throws `Exception` with `VoxenErrc::ExternalLibFailure` on error.
	// NOTE: this function can be called only from the main thread; this is not validated.
	static void initLibrary();
	// Terminate the windowing library. No live window object must remain before this call.
	// NOTE: this function can be called only from the main thread; this is not validated.
	static void terminateLibrary() noexcept;

private:
	SDL_Window *m_window = nullptr;
	uint32_t m_window_id = 0;
	bool m_should_close = false;

	// Filled when processing OS events, moved to `m_events` by `pollEvents()`
	std::vector<WindowEvent> m_pending_events;
	std::vector<WindowEvent> m_events;

	VOXEN_LOCAL void swap(Window &other) noexcept;
	VOXEN_LOCAL void handleSdlEvent(const SDL_Event &event);
	VOXEN_LOCAL m::double2 toFramebufferCoords(float x, float y) const;
};

} // namespace voxen::os
