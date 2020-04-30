#pragma once

#include <cstdint>

struct GLFWwindow;

namespace voxen
{

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

	void logGlfwVersion() const;
	void createWindow();

	static void glfwErrorCallback(int code, const char *message) noexcept;
};

}
