#include <voxen/client/window.hpp>
#include <voxen/util/log.hpp>
#include <voxen/common/gui.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace voxen
{

Window Window::gInstance;

void Window::start () {
	if (mIsStarted)
		return;
	Log::info("Starting window and GLFW library");
	logGlfwVersion ();
	glfwSetErrorCallback (&Window::glfwErrorCallback);
	if (glfwInit () != GLFW_TRUE) {
		Log::fatal("Couldn't init GLFW!");
		throw std::runtime_error ("GLFW init failed");
	}
	createWindow ();
	glfwSetWindowUserPointer(mWindow, this);
	//TODO own methods for this stuff and the stuff changing
	//glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetInputMode(mWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	Log::info("GLFW started successfully, window created");
	mIsStarted = true;
}

void Window::stop () {
	if (!mIsStarted)
		return;
	Log::info("Destroying window and stopping GLFW");
	glfwDestroyWindow (mWindow);
	glfwTerminate ();
	mIsStarted = false;
}

bool Window::shouldClose () const {
	return glfwWindowShouldClose (mWindow) != 0;
}

void Window::pollEvents () {
	glfwPollEvents ();
}

void Window::logGlfwVersion () const {
	Log::debug("Voxen is compiled against GLFW {}.{}.{} ({})",
		GLFW_VERSION_MAJOR, GLFW_VERSION_MINOR, GLFW_VERSION_REVISION, glfwGetVersionString ());
	int major, minor, revision;
	glfwGetVersion (&major, &minor, &revision);
	Log::debug("Voxen is running against GLFW {}.{}.{}", major, minor, revision);
}

void Window::createWindow () {
	glfwWindowHint (GLFW_RESIZABLE, GLFW_TRUE); // for windowed
	glfwWindowHint (GLFW_FOCUSED, GLFW_TRUE); // for windowed
	glfwWindowHint (GLFW_AUTO_ICONIFY, GLFW_TRUE); // for full-screen
	glfwWindowHint (GLFW_CENTER_CURSOR, GLFW_TRUE); // for full-screen
	glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
	// TODO: this is a temporary hack to simplify Vulkan logic. Remove it
	glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
	mWindow = glfwCreateWindow (k_width, k_height, "Voxen", nullptr, nullptr);
	if (!mWindow) {
		Log::fatal("Couldn't create window!");
		glfwTerminate ();
		throw std::runtime_error ("window creation failed");
	}
}

void Window::glfwErrorCallback (int code, const char *message) noexcept {
	Log::error("GLFW error {}:\n{}", code, message);
}

bool Window::attachGUI(voxen::Gui& gui)
{
	if (m_attached_gui != nullptr)
		throw std::runtime_error ("try to attach gui to window, which already have another attached gui");
	m_attached_gui = &gui;
	glfwSetKeyCallback(mWindow, Window::globalKeyCallback);
	glfwSetCursorPosCallback(mWindow, Window::globalMouseMovement);
	glfwSetMouseButtonCallback(mWindow, Window::globalMouseKey);
	glfwSetScrollCallback(mWindow, Window::globalMouseScroll);
	return true;
}

void Window::globalMouseMovement (GLFWwindow* window, double xpos, double ypos) noexcept
{
	void* ptr = glfwGetWindowUserPointer(window);
	   Gui* gui = static_cast<Window*>(ptr)->m_attached_gui;
	if (gui)
		gui->handleCursor(xpos, ypos);
}

void Window::globalKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept
{
	void* ptr = glfwGetWindowUserPointer(window);
	   Gui* gui = static_cast<Window*>(ptr)->m_attached_gui;
	if (gui)
		gui->handleKey(key, scancode, action, mods);
}

void Window::globalMouseKey(GLFWwindow* window, int button, int action, int mods) noexcept
{
	void* ptr = glfwGetWindowUserPointer(window);
	   Gui* gui = static_cast<Window*>(ptr)->m_attached_gui;
	if (gui)
		gui->handleMouseKey(button, action, mods);
}

void Window::globalMouseScroll(GLFWwindow* window, double xoffset, double yoffset) noexcept
{
	void* ptr = glfwGetWindowUserPointer(window);
	   Gui* gui = static_cast<Window*>(ptr)->m_attached_gui;
	if (gui)
		gui->handleMouseScroll(xoffset, yoffset);
}

int Window::width() const noexcept
{
	int width;
	glfwGetWindowSize(mWindow, &width, NULL);
	return width;
}

int Window::height() const noexcept
{
	int height;
	glfwGetWindowSize(mWindow, NULL, &height);
	return height;
}

std::pair<double, double> Window::cursorPos() const noexcept
{
	double xpos, ypos;
	glfwGetCursorPos(mWindow, &xpos, &ypos);
	return std::make_pair(xpos, ypos);
}

}
