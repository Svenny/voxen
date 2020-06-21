#pragma once

#include <cstdint>
#include <utility>

struct GLFWwindow;

namespace voxen::client
{

class Gui;

class Window {
public:
	bool shouldClose() const;
	void pollEvents();

	void start(int width, int height);
	void stop();
	bool isStarted() const noexcept { return mIsStarted; }

	GLFWwindow *glfwHandle() const noexcept { return mWindow; }

	bool attachGUI(Gui& gui);

	int width() const noexcept;
	int height() const noexcept;
	// Note: may be not equal to (width(), height()) because window
	// size is measured in logical units while framebuffer is in pixels.
	// See docs for `glfwGetWindowSize` and `glfwGetFramebufferSize`.
	std::pair<int, int> framebufferSize() const noexcept;

	std::pair<double, double> cursorPos() const noexcept;

	void useRegularCursor();
	void useOrientationCursor();

	static Window &instance() noexcept { return gInstance; }
private:
	Window() = default;
	~Window() = default;
	Window(const Window &) = delete;
	Window &operator = (const Window &) = delete;
	Window(Window &&) = delete;
	Window &operator = (Window &&) = delete;

	static Window gInstance;
	bool mIsStarted = false;

	GLFWwindow *mWindow;
	Gui* m_attached_gui = nullptr;

	void logGlfwVersion() const;
	void createWindow(int width, int height);

	static void globalKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept;
	static void globalMouseMovement(GLFWwindow* window, double xpos, double ypos) noexcept;
	static void globalMouseKey(GLFWwindow* window, int button, int action, int mods) noexcept;
	static void globalMouseScroll(GLFWwindow* window, double xoffset, double yoffset) noexcept;

	static void glfwErrorCallback(int code, const char *message) noexcept;
};

}
