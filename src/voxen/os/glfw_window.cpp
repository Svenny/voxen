#include <voxen/os/glfw_window.hpp>

#include <voxen/client/gui.hpp>
#include <voxen/os/futex.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <GLFW/glfw3.h>

namespace voxen::os
{

namespace
{

void logGlfwVersion()
{
	Log::debug("Voxen is compiled against GLFW {}.{}.{}", GLFW_VERSION_MAJOR, GLFW_VERSION_MINOR, GLFW_VERSION_REVISION);

	int major, minor, revision;
	glfwGetVersion(&major, &minor, &revision);
	Log::debug("Voxen is running against GLFW {}.{}.{} ({})", major, minor, revision, glfwGetVersionString());
}

} // namespace

GlfwWindow::GlfwWindow(Config cfg)
{
	using namespace std::literals;

	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);       // Only for windowed
	glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);          // Only for windowed
	glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_TRUE);     // Only for full-screen
	glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);    // Only for full-screen
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);     // Vulkan!
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE); // Kinda unify X11 and Wayland behaviors

	GLFWmonitor* monitor = nullptr;
	if (cfg.fullscreen) {
		// TODO: add possibility to select non-primary monitor?
		monitor = glfwGetPrimaryMonitor();

		const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
		// Match video mode dimensions to get "borderless fullscreen" mode
		cfg.width = vidmode->width;
		cfg.height = vidmode->height;
		// Not sure if setting this is needed with Vulkan surface but why not
		glfwWindowHint(GLFW_RED_BITS, vidmode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, vidmode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, vidmode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, vidmode->refreshRate);
	}

	m_window = glfwCreateWindow(cfg.width, cfg.height, cfg.title, monitor, nullptr);

	if (!m_window) {
		Log::fatal("Can't create GLFW window!");
		throw Exception::fromError(VoxenErrc::ExternalLibFailure, "glfwCreateWindow failed");
	}

	glfwSetWindowUserPointer(m_window, this);
	useRegularCursor();
}

GlfwWindow::~GlfwWindow() noexcept
{
	glfwDestroyWindow(m_window);
}

bool GlfwWindow::shouldClose() const
{
	return glfwWindowShouldClose(m_window) != 0;
}

void GlfwWindow::pollEvents()
{
	glfwPollEvents();
}

std::pair<int, int> GlfwWindow::waitUntilUnMinimized()
{
	auto size = framebufferSize();
	while (size.first == 0 && size.second == 0 && !shouldClose()) {
		glfwWaitEvents();
		size = framebufferSize();
	}

	return size;
}

void GlfwWindow::glfwErrorCallback(int code, const char* message) noexcept
{
	Log::error("GLFW error {}:\n{}", code, message);
}

// TODO: eliminate this strange dependency
using client::Gui;

bool GlfwWindow::attachGUI(Gui& gui)
{
	if (m_attached_gui != nullptr) {
		throw std::runtime_error("try to attach gui to window, which already have another attached gui");
	}
	m_attached_gui = &gui;
	glfwSetKeyCallback(m_window, GlfwWindow::globalKeyCallback);
	glfwSetCursorPosCallback(m_window, GlfwWindow::globalMouseMovement);
	glfwSetMouseButtonCallback(m_window, GlfwWindow::globalMouseKey);
	glfwSetScrollCallback(m_window, GlfwWindow::globalMouseScroll);
	return true;
}

void GlfwWindow::globalMouseMovement(GLFWwindow* window, double xpos, double ypos) noexcept
{
	void* ptr = glfwGetWindowUserPointer(window);
	Gui* gui = static_cast<GlfwWindow*>(ptr)->m_attached_gui;
	if (gui) {
		gui->handleCursor(xpos, ypos);
	}
}

void GlfwWindow::globalKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept
{
	void* ptr = glfwGetWindowUserPointer(window);
	Gui* gui = static_cast<GlfwWindow*>(ptr)->m_attached_gui;
	if (gui) {
		gui->handleKey(key, scancode, action, mods);
	}
}

void GlfwWindow::globalMouseKey(GLFWwindow* window, int button, int action, int mods) noexcept
{
	void* ptr = glfwGetWindowUserPointer(window);
	Gui* gui = static_cast<GlfwWindow*>(ptr)->m_attached_gui;
	if (gui) {
		gui->handleMouseKey(button, action, mods);
	}
}

void GlfwWindow::globalMouseScroll(GLFWwindow* window, double xoffset, double yoffset) noexcept
{
	void* ptr = glfwGetWindowUserPointer(window);
	Gui* gui = static_cast<GlfwWindow*>(ptr)->m_attached_gui;
	if (gui) {
		gui->handleMouseScroll(xoffset, yoffset);
	}
}

std::pair<int, int> GlfwWindow::windowSize() const
{
	int width, height;
	glfwGetWindowSize(m_window, &width, &height);
	return { width, height };
}

std::pair<int, int> GlfwWindow::framebufferSize() const
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	return { width, height };
}

std::pair<double, double> GlfwWindow::cursorPos() const
{
	double xpos, ypos;
	glfwGetCursorPos(m_window, &xpos, &ypos);
	return std::make_pair(xpos, ypos);
}

void GlfwWindow::useRegularCursor()
{
	glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	if (glfwRawMouseMotionSupported()) {
		glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
	}
}

void GlfwWindow::useGrabbedCursor()
{
	glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported()) {
		glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}
}

void GlfwWindow::initGlfw()
{
	Log::info("Initializing GLFW library");
	logGlfwVersion();

	glfwSetErrorCallback(&GlfwWindow::glfwErrorCallback);

	if (glfwPlatformSupported(GLFW_PLATFORM_WAYLAND)) {
		// Use Wayland if available
		glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
	} else if (glfwPlatformSupported(GLFW_PLATFORM_X11)) {
		// Otherwise use X11 with XCB surface
		glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
		glfwInitHint(GLFW_X11_XCB_VULKAN_SURFACE, GLFW_TRUE);
	}

	if (glfwInit() != GLFW_TRUE) {
		Log::fatal("Can't init GLFW!");
		throw Exception::fromError(VoxenErrc::ExternalLibFailure, "GLFW init failed");
	}

	Log::info("GLFW initialized successfully");
}

void GlfwWindow::terminateGlfw() noexcept
{
	Log::info("Terminating GLFW library");
	glfwTerminate();
}

} // namespace voxen::os
