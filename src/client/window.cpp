#include <voxen/client/window.hpp>
#include <voxen/client/gui.hpp>
#include <voxen/util/log.hpp>
#include <voxen/common/config.hpp>

#include <GLFW/glfw3.h>

namespace voxen::client
{

Window Window::gInstance;

void Window::start (int width, int height) {
	if (mIsStarted)
		return;
	Log::info("Starting window and GLFW library");
	logGlfwVersion ();
	glfwSetErrorCallback (&Window::glfwErrorCallback);
	if (glfwInit () != GLFW_TRUE) {
		Log::fatal("Couldn't init GLFW!");
		throw std::runtime_error ("GLFW init failed");
	}
	createWindow (width, height);
	useRegularCursor();
	glfwSetWindowUserPointer(mWindow, this);
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

void Window::createWindow (int width, int height) {
	using namespace std::literals;

	glfwWindowHint (GLFW_RESIZABLE, GLFW_TRUE); // for windowed
	glfwWindowHint (GLFW_FOCUSED, GLFW_TRUE); // for windowed
	glfwWindowHint (GLFW_AUTO_ICONIFY, GLFW_TRUE); // for full-screen
	glfwWindowHint (GLFW_CENTER_CURSOR, GLFW_TRUE); // for full-screen
	glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);

	Config *cfg = Config::mainConfig();
	GLFWmonitor *monitor = nullptr;
	if (cfg->optionBool("window"sv, "fullscreen"sv)) {
		// TODO: add possibility select non-primary monitor?
		monitor = glfwGetPrimaryMonitor();
	}
	mWindow = glfwCreateWindow (width, height, "Voxen", monitor, nullptr);

	if (!mWindow) {
		Log::fatal("Couldn't create window!");
		glfwTerminate ();
		throw std::runtime_error ("window creation failed");
	}
}

void Window::glfwErrorCallback (int code, const char *message) noexcept {
	Log::error("GLFW error {}:\n{}", code, message);
}

bool Window::attachGUI(Gui& gui)
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
	glfwGetWindowSize(mWindow, &width, nullptr);
	return width;
}

int Window::height() const noexcept
{
	int height;
	glfwGetWindowSize(mWindow, nullptr, &height);
	return height;
}

std::pair<int, int> Window::framebufferSize() const noexcept
{
	int width, height;
	glfwGetFramebufferSize(mWindow, &width, &height);
	return { width, height };
}

std::pair<double, double> Window::cursorPos() const noexcept
{
	double xpos, ypos;
	glfwGetCursorPos(mWindow, &xpos, &ypos);
	return std::make_pair(xpos, ypos);
}

void Window::useRegularCursor()
{
	glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(mWindow, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
}

void Window::useOrientationCursor() {
	glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(mWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
}


}
