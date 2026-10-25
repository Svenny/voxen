#include <voxen/os/window.hpp>

// GLFW declares its Vulkan functions only when Vulkan headers are included before it
#include <voxen/gfx/vk/vk_include.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <m/vector.hpp>

#include <GLFW/glfw3.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

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

void glfwErrorCallback(int code, const char *message) noexcept
{
	Log::error("GLFW error {}:\n{}", code, message);
}

Key translateKey(int key) noexcept
{
	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
		return Key(int(Key::A) + key - GLFW_KEY_A);
	}
	if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
		return Key(int(Key::Digit1) + key - GLFW_KEY_1);
	}
	if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) {
		return Key(int(Key::F1) + key - GLFW_KEY_F1);
	}
	if (key >= GLFW_KEY_F13 && key <= GLFW_KEY_F24) {
		return Key(int(Key::F13) + key - GLFW_KEY_F13);
	}
	if (key >= GLFW_KEY_KP_1 && key <= GLFW_KEY_KP_9) {
		return Key(int(Key::Kp1) + key - GLFW_KEY_KP_1);
	}

	switch (key) {
	case GLFW_KEY_0:
		return Key::Digit0;
	case GLFW_KEY_ENTER:
		return Key::Enter;
	case GLFW_KEY_ESCAPE:
		return Key::Escape;
	case GLFW_KEY_BACKSPACE:
		return Key::Backspace;
	case GLFW_KEY_TAB:
		return Key::Tab;
	case GLFW_KEY_SPACE:
		return Key::Space;
	case GLFW_KEY_MINUS:
		return Key::Minus;
	case GLFW_KEY_EQUAL:
		return Key::Equal;
	case GLFW_KEY_LEFT_BRACKET:
		return Key::LeftBracket;
	case GLFW_KEY_RIGHT_BRACKET:
		return Key::RightBracket;
	case GLFW_KEY_BACKSLASH:
		return Key::Backslash;
	case GLFW_KEY_SEMICOLON:
		return Key::Semicolon;
	case GLFW_KEY_APOSTROPHE:
		return Key::Apostrophe;
	case GLFW_KEY_GRAVE_ACCENT:
		return Key::Grave;
	case GLFW_KEY_COMMA:
		return Key::Comma;
	case GLFW_KEY_PERIOD:
		return Key::Period;
	case GLFW_KEY_SLASH:
		return Key::Slash;
	case GLFW_KEY_CAPS_LOCK:
		return Key::CapsLock;
	case GLFW_KEY_PRINT_SCREEN:
		return Key::PrintScreen;
	case GLFW_KEY_SCROLL_LOCK:
		return Key::ScrollLock;
	case GLFW_KEY_PAUSE:
		return Key::Pause;
	case GLFW_KEY_INSERT:
		return Key::Insert;
	case GLFW_KEY_HOME:
		return Key::Home;
	case GLFW_KEY_PAGE_UP:
		return Key::PageUp;
	case GLFW_KEY_DELETE:
		return Key::Delete;
	case GLFW_KEY_END:
		return Key::End;
	case GLFW_KEY_PAGE_DOWN:
		return Key::PageDown;
	case GLFW_KEY_RIGHT:
		return Key::Right;
	case GLFW_KEY_LEFT:
		return Key::Left;
	case GLFW_KEY_DOWN:
		return Key::Down;
	case GLFW_KEY_UP:
		return Key::Up;
	case GLFW_KEY_NUM_LOCK:
		return Key::NumLock;
	case GLFW_KEY_KP_DIVIDE:
		return Key::KpDivide;
	case GLFW_KEY_KP_MULTIPLY:
		return Key::KpMultiply;
	case GLFW_KEY_KP_SUBTRACT:
		return Key::KpSubtract;
	case GLFW_KEY_KP_ADD:
		return Key::KpAdd;
	case GLFW_KEY_KP_ENTER:
		return Key::KpEnter;
	case GLFW_KEY_KP_0:
		return Key::Kp0;
	case GLFW_KEY_KP_DECIMAL:
		return Key::KpDecimal;
	case GLFW_KEY_KP_EQUAL:
		return Key::KpEqual;
	case GLFW_KEY_WORLD_2:
		return Key::NonUsBackslash;
	case GLFW_KEY_MENU:
		return Key::Menu;
	case GLFW_KEY_LEFT_CONTROL:
		return Key::LeftControl;
	case GLFW_KEY_LEFT_SHIFT:
		return Key::LeftShift;
	case GLFW_KEY_LEFT_ALT:
		return Key::LeftAlt;
	case GLFW_KEY_LEFT_SUPER:
		return Key::LeftSuper;
	case GLFW_KEY_RIGHT_CONTROL:
		return Key::RightControl;
	case GLFW_KEY_RIGHT_SHIFT:
		return Key::RightShift;
	case GLFW_KEY_RIGHT_ALT:
		return Key::RightAlt;
	case GLFW_KEY_RIGHT_SUPER:
		return Key::RightSuper;
	default:
		return Key::Unknown;
	}
}

KeyMods translateMods(int mods) noexcept
{
	KeyMods result;
	if (mods & GLFW_MOD_SHIFT) {
		result |= KeyModBit::Shift;
	}
	if (mods & GLFW_MOD_CONTROL) {
		result |= KeyModBit::Control;
	}
	if (mods & GLFW_MOD_ALT) {
		result |= KeyModBit::Alt;
	}
	if (mods & GLFW_MOD_SUPER) {
		result |= KeyModBit::Super;
	}
	if (mods & GLFW_MOD_CAPS_LOCK) {
		result |= KeyModBit::CapsLock;
	}
	if (mods & GLFW_MOD_NUM_LOCK) {
		result |= KeyModBit::NumLock;
	}
	return result;
}

bool translateMouseButton(int button, MouseButton &result) noexcept
{
	switch (button) {
	case GLFW_MOUSE_BUTTON_LEFT:
		result = MouseButton::Left;
		return true;
	case GLFW_MOUSE_BUTTON_RIGHT:
		result = MouseButton::Right;
		return true;
	case GLFW_MOUSE_BUTTON_MIDDLE:
		result = MouseButton::Middle;
		return true;
	case GLFW_MOUSE_BUTTON_4:
		result = MouseButton::X1;
		return true;
	case GLFW_MOUSE_BUTTON_5:
		result = MouseButton::X2;
		return true;
	default:
		return false;
	}
}

void appendUtf8(std::string &str, uint32_t cp)
{
	if (cp < 0x80) {
		str += char(cp);
	} else if (cp < 0x800) {
		str += char(0xC0 | (cp >> 6));
		str += char(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		str += char(0xE0 | (cp >> 12));
		str += char(0x80 | ((cp >> 6) & 0x3F));
		str += char(0x80 | (cp & 0x3F));
	} else {
		str += char(0xF0 | (cp >> 18));
		str += char(0x80 | ((cp >> 12) & 0x3F));
		str += char(0x80 | ((cp >> 6) & 0x3F));
		str += char(0x80 | (cp & 0x3F));
	}
}

Window *getWindow(GLFWwindow *window) noexcept
{
	return static_cast<Window *>(glfwGetWindowUserPointer(window));
}

} // namespace

Window::Window(Config cfg)
{
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);       // Only for windowed
	glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);          // Only for windowed
	glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_TRUE);     // Only for full-screen
	glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);    // Only for full-screen
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);     // Vulkan!
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE); // Kinda unify X11 and Wayland behaviors

	GLFWmonitor *monitor = nullptr;
	if (cfg.fullscreen) {
		// TODO: add possibility to select non-primary monitor?
		monitor = glfwGetPrimaryMonitor();

		const GLFWvidmode *vidmode = glfwGetVideoMode(monitor);
		// Match video mode dimensions to get "borderless fullscreen" mode
		cfg.width = vidmode->width;
		cfg.height = vidmode->height;
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
	m_last_cursor_pos = cursorPos();

	glfwSetKeyCallback(m_window, [](GLFWwindow *window, int key, int /*scancode*/, int action, int mods) noexcept {
		getWindow(window)->pushEvent(KeyEvent {
			.key = translateKey(key),
			.mods = translateMods(mods),
			.pressed = action != GLFW_RELEASE,
			.repeat = action == GLFW_REPEAT,
		});
	});

	glfwSetCharCallback(m_window, [](GLFWwindow *window, unsigned int codepoint) noexcept {
		TextInputEvent event;
		appendUtf8(event.text, codepoint);
		getWindow(window)->pushEvent(std::move(event));
	});

	glfwSetMouseButtonCallback(m_window, [](GLFWwindow *window, int button, int action, int mods) noexcept {
		MouseButton btn;
		if (!translateMouseButton(button, btn)) {
			return;
		}

		Window *self = getWindow(window);
		self->pushEvent(MouseButtonEvent {
			.button = btn,
			.mods = translateMods(mods),
			.pressed = action != GLFW_RELEASE,
			.clicks = action != GLFW_RELEASE ? 1 : 0,
			.position = self->cursorPos(),
		});
	});

	glfwSetCursorPosCallback(m_window, [](GLFWwindow *window, double xpos, double ypos) noexcept {
		Window *self = getWindow(window);
		m::double2 pos = self->toFramebufferCoords(xpos, ypos);
		self->pushEvent(MouseMotionEvent {
			.position = pos,
			.delta = pos - self->m_last_cursor_pos,
		});
		self->m_last_cursor_pos = pos;
	});

	glfwSetScrollCallback(m_window, [](GLFWwindow *window, double xoffset, double yoffset) noexcept {
		getWindow(window)->pushEvent(MouseWheelEvent { .delta = m::double2(xoffset, yoffset) });
	});

	glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow *window, int width, int height) noexcept {
		getWindow(window)->pushEvent(WindowResizeEvent { .framebuffer_size = { width, height } });
	});

	glfwSetWindowCloseCallback(m_window,
		[](GLFWwindow *window) noexcept { getWindow(window)->pushEvent(WindowCloseEvent {}); });

	useRegularCursor();
}

Window::Window(Window &&other) noexcept
{
	swap(other);
}

Window &Window::operator=(Window &&other) noexcept
{
	swap(other);
	return *this;
}

Window::~Window() noexcept
{
	glfwDestroyWindow(m_window);
}

bool Window::shouldClose() const
{
	return glfwWindowShouldClose(m_window) != 0;
}

void Window::pollEvents()
{
	glfwPollEvents();
	m_events.clear();
	std::swap(m_events, m_pending_events);
}

Resolution Window::waitUntilUnMinimized()
{
	auto size = framebufferSize();
	while ((size.width == 0 || size.height == 0) && !shouldClose()) {
		glfwWaitEvents();
		size = framebufferSize();
	}

	return size;
}

Resolution Window::windowSize() const
{
	int width, height;
	glfwGetWindowSize(m_window, &width, &height);
	return { width, height };
}

Resolution Window::framebufferSize() const
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	return { width, height };
}

m::double2 Window::cursorPos() const
{
	double xpos, ypos;
	glfwGetCursorPos(m_window, &xpos, &ypos);
	return toFramebufferCoords(xpos, ypos);
}

bool Window::mouseButtonPressed(MouseButton button) const
{
	static constexpr int BUTTONS[] = {
		GLFW_MOUSE_BUTTON_LEFT,
		GLFW_MOUSE_BUTTON_RIGHT,
		GLFW_MOUSE_BUTTON_MIDDLE,
		GLFW_MOUSE_BUTTON_4,
		GLFW_MOUSE_BUTTON_5,
	};
	return glfwGetMouseButton(m_window, BUTTONS[size_t(button)]) == GLFW_PRESS;
}

void Window::useRegularCursor()
{
	glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	if (glfwRawMouseMotionSupported()) {
		glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
	}
}

void Window::useGrabbedCursor()
{
	glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported()) {
		glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}
}

void Window::startTextInput()
{
	// GLFW always delivers text input
}

void Window::stopTextInput() {}

bool Window::createVulkanSurface(VkInstance instance, VkSurfaceKHR *surface) const
{
	VkResult res = glfwCreateWindowSurface(instance, m_window, nullptr, surface);
	if (res != VK_SUCCESS) {
		Log::error("glfwCreateWindowSurface failed: {}", int(res));
		return false;
	}

	return true;
}

std::span<const char *const> Window::requiredVulkanInstanceExtensions()
{
	uint32_t count = 0;
	const char **list = glfwGetRequiredInstanceExtensions(&count);
	return { list, count };
}

bool Window::vulkanPresentationSupport(VkInstance instance, VkPhysicalDevice physical_device, uint32_t queue_family)
{
	return glfwGetPhysicalDevicePresentationSupport(instance, physical_device, queue_family) == GLFW_TRUE;
}

void Window::initLibrary()
{
	Log::info("Initializing GLFW library");
	logGlfwVersion();

	glfwSetErrorCallback(&glfwErrorCallback);

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

void Window::terminateLibrary() noexcept
{
	Log::info("Terminating GLFW library");
	glfwTerminate();
}

void Window::swap(Window &other) noexcept
{
	// User pointers point to the owning objects
	if (m_window) {
		glfwSetWindowUserPointer(m_window, &other);
	}
	if (other.m_window) {
		glfwSetWindowUserPointer(other.m_window, this);
	}

	std::swap(m_window, other.m_window);
	std::swap(m_pending_events, other.m_pending_events);
	std::swap(m_events, other.m_events);
	std::swap(m_last_cursor_pos, other.m_last_cursor_pos);
}

void Window::pushEvent(WindowEvent event)
{
	m_pending_events.emplace_back(std::move(event));
}

m::double2 Window::toFramebufferCoords(double x, double y) const
{
	// GLFW reports cursor positions in window (logical) coordinates
	Resolution window_size = windowSize();
	Resolution fb_size = framebufferSize();
	if (window_size.width <= 0 || window_size.height <= 0) {
		return m::double2(x, y);
	}

	return m::double2(x * fb_size.width / window_size.width, y * fb_size.height / window_size.height);
}

} // namespace voxen::os
