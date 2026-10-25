#pragma once

#include <voxen/os/window_events.hpp>
#include <voxen/util/resolution.hpp>
#include <voxen/visibility.hpp>

#include <m/vector.hpp>

#include <cstdint>
#include <span>
#include <vector>

struct GLFWwindow;

// Vulkan handles exactly as `vulkan_core.h` declares them on 64-bit platforms, avoid including it here
using VkInstance = struct VkInstance_T *;
using VkPhysicalDevice = struct VkPhysicalDevice_T *;
using VkSurfaceKHR = struct VkSurfaceKHR_T *;

namespace voxen::os
{

// OS window, the source of input events. Its methods can be called only from the main thread.
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

	// Block until the window is expanded from the minimized state
	// then return the new value of `framebufferSize()`. Call
	// this when creating a surface/swapchain and the size is zero.
	// Events received while waiting are returned by the next `pollEvents()`.
	Resolution waitUntilUnMinimized();

	// Window size in logical units. May differ from `framebufferSize()`
	// when the OS scales windows (e.g. Wayland with fractional scaling).
	// NOTE: can return empty (zeros) if the window is minimized.
	Resolution windowSize() const;
	// Window framebuffer size in pixels. Usually you need this function.
	// NOTE: can return empty (zeros) if the window is minimized.
	Resolution framebufferSize() const;

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
	GLFWwindow *m_window = nullptr;

	// Filled by OS event callbacks, moved to `m_events` by `pollEvents()`
	std::vector<WindowEvent> m_pending_events;
	std::vector<WindowEvent> m_events;
	// To compute relative motion
	m::double2 m_last_cursor_pos;

	VOXEN_LOCAL void swap(Window &other) noexcept;
	VOXEN_LOCAL void pushEvent(WindowEvent event);
	VOXEN_LOCAL m::double2 toFramebufferCoords(double x, double y) const;
};

} // namespace voxen::os
