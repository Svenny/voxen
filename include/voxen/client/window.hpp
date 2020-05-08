#pragma once

#include <cstdint>
#include <utility>

struct GLFWwindow;

namespace voxen
{

class Gui;

class Window {
public:
	constexpr static uint32_t k_width = 1600;
	constexpr static uint32_t k_height = 900;

	bool shouldClose() const;
	void pollEvents();

	void start();
	void stop();
	bool isStarted() const noexcept { return mIsStarted; }

	GLFWwindow *glfwHandle() const noexcept { return mWindow; }

	bool attachGUI(Gui& gui);

	int width() const noexcept;
	int height() const noexcept;

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
	void createWindow();

	static void globalKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept;
	static void globalMouseMovement(GLFWwindow* window, double xpos, double ypos) noexcept;
	static void globalMouseKey(GLFWwindow* window, int button, int action, int mods) noexcept;
	static void globalMouseScroll(GLFWwindow* window, double xoffset, double yoffset) noexcept;

	static void glfwErrorCallback(int code, const char *message) noexcept;
};

}
