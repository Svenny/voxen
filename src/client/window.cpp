#include <voxen/client/window.hpp>
#include <voxen/util/log.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>


//extern "C" GLFWAPI VkResult glfwCreateWindowSurface(VkInstance instance, GLFWwindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);

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
	//glfwWindowHint (GLFW_CENTER_CURSOR, GLFW_TRUE); // for full-screen, TODO: uncomment?
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

}
