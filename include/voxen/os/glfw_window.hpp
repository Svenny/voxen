#pragma once

#include <voxen/visibility.hpp>

#include <utility>

struct GLFWwindow;

namespace voxen::client
{

class Gui;

}

namespace voxen::os
{

class VOXEN_API GlfwWindow {
public:
	struct Config {
		int width = 0;
		int height = 0;
		const char *title = nullptr;
		bool fullscreen = false;
	};

	explicit GlfwWindow(Config cfg);
	GlfwWindow(GlfwWindow &&) = delete;
	GlfwWindow(const GlfwWindow &) = delete;
	GlfwWindow &operator=(GlfwWindow &&) = delete;
	GlfwWindow &operator=(const GlfwWindow &) = delete;
	~GlfwWindow() noexcept;

	bool shouldClose() const;
	void pollEvents();
	// Block until the window is expanded from the minimized state
	// then return the new value of `framebufferSize()`. Call
	// this when creating a surface/swapchain and the size is zero.
	std::pair<int, int> waitUntilUnMinimized();

	GLFWwindow *glfwHandle() const noexcept { return m_window; }

	bool attachGUI(client::Gui &gui);

	// Window size in logcial units, see docs for `glfwGetWindowSize`.
	// NOTE: may be different from `framebufferSize()` because window
	// size is measured in logical units while framebuffer is in pixels.
	// See docs for `glfwGetWindowSize` and `glfwGetFramebufferSize`.
	std::pair<int, int> windowSize() const;
	// Window framebuffer size in pixels. Usually you need this function.
	std::pair<int, int> framebufferSize() const;

	std::pair<double, double> cursorPos() const;

	void useRegularCursor();
	void useGrabbedCursor();

	// Initialize GLFW library. Throws `Exception` with `VoxenErrc::ExternalLibFailure` on error.
	// NOTE: this function can be called only from the main thread; this is not validated.
	static void initGlfw();
	// Terminate GLFW library. No live window object must remain before this call.
	// NOTE: this function can be called only from the main thread; this is not validated.
	static void terminateGlfw() noexcept;

private:
	GLFWwindow *m_window = nullptr;
	client::Gui *m_attached_gui = nullptr;

	VOXEN_LOCAL void createWindow(int width, int height);

	VOXEN_LOCAL static void globalKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) noexcept;
	VOXEN_LOCAL static void globalMouseMovement(GLFWwindow *window, double xpos, double ypos) noexcept;
	VOXEN_LOCAL static void globalMouseKey(GLFWwindow *window, int button, int action, int mods) noexcept;
	VOXEN_LOCAL static void globalMouseScroll(GLFWwindow *window, double xoffset, double yoffset) noexcept;

	VOXEN_LOCAL static void glfwErrorCallback(int code, const char *message) noexcept;
};

} // namespace voxen::os
