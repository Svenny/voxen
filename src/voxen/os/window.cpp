#include <voxen/os/window.hpp>

#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <m/vector.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_version.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace voxen::os
{

namespace
{

void logSdlVersion()
{
	Log::debug("Voxen is compiled against SDL {}.{}.{}", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);

	int version = SDL_GetVersion();
	Log::debug("Voxen is running against SDL {}.{}.{} ({})", SDL_VERSIONNUM_MAJOR(version),
		SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version), SDL_GetRevision());
}

void sdlLogCallback(void * /*userdata*/, int category, SDL_LogPriority priority, const char *message) noexcept
{
	switch (priority) {
	case SDL_LOG_PRIORITY_TRACE:
	case SDL_LOG_PRIORITY_VERBOSE:
		Log::trace("SDL [{}]: {}", category, message);
		break;
	case SDL_LOG_PRIORITY_DEBUG:
		Log::debug("SDL [{}]: {}", category, message);
		break;
	case SDL_LOG_PRIORITY_INFO:
		Log::info("SDL [{}]: {}", category, message);
		break;
	case SDL_LOG_PRIORITY_WARN:
		Log::warn("SDL [{}]: {}", category, message);
		break;
	default:
		Log::error("SDL [{}]: {}", category, message);
		break;
	}
}

Key translateKey(SDL_Scancode scancode) noexcept
{
	// `SDL_Scancode` values are USB HID usage IDs, like ours
	auto code = static_cast<uint32_t>(scancode);
	if ((code >= uint32_t(Key::A) && code <= uint32_t(Key::Menu))
		|| (code >= uint32_t(Key::KpEqual) && code <= uint32_t(Key::F24))
		|| (code >= uint32_t(Key::LeftControl) && code <= uint32_t(Key::RightSuper))) {
		return Key(code);
	}

	return Key::Unknown;
}

KeyMods translateMods(SDL_Keymod mod) noexcept
{
	KeyMods result;
	if (mod & SDL_KMOD_SHIFT) {
		result |= KeyModBit::Shift;
	}
	if (mod & SDL_KMOD_CTRL) {
		result |= KeyModBit::Control;
	}
	if (mod & SDL_KMOD_ALT) {
		result |= KeyModBit::Alt;
	}
	if (mod & SDL_KMOD_GUI) {
		result |= KeyModBit::Super;
	}
	if (mod & SDL_KMOD_CAPS) {
		result |= KeyModBit::CapsLock;
	}
	if (mod & SDL_KMOD_NUM) {
		result |= KeyModBit::NumLock;
	}
	return result;
}

bool translateMouseButton(uint8_t button, MouseButton &result) noexcept
{
	switch (button) {
	case SDL_BUTTON_LEFT:
		result = MouseButton::Left;
		return true;
	case SDL_BUTTON_RIGHT:
		result = MouseButton::Right;
		return true;
	case SDL_BUTTON_MIDDLE:
		result = MouseButton::Middle;
		return true;
	case SDL_BUTTON_X1:
		result = MouseButton::X1;
		return true;
	case SDL_BUTTON_X2:
		result = MouseButton::X2;
		return true;
	default:
		return false;
	}
}

int toSdlMouseButton(MouseButton button) noexcept
{
	switch (button) {
	case MouseButton::Left:
		return SDL_BUTTON_LEFT;
	case MouseButton::Right:
		return SDL_BUTTON_RIGHT;
	case MouseButton::Middle:
		return SDL_BUTTON_MIDDLE;
	case MouseButton::X1:
		return SDL_BUTTON_X1;
	case MouseButton::X2:
		return SDL_BUTTON_X2;
	case MouseButton::EnumSize:
		break;
	}

	return 0;
}

} // namespace

Window::Window(Config cfg)
{
	// Hidden until sized properly. Borderless fullscreen when no display mode is set. Resizable,
	// otherwise SDL on Win32 pins the window size, even against programmatic resizes.
	SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
	if (cfg.fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}

	m_window = SDL_CreateWindow(cfg.title, cfg.width, cfg.height, flags);
	if (!m_window) {
		Log::fatal("Can't create window: {}", SDL_GetError());
		throw Exception::fromError(VoxenErrc::ExternalLibFailure, "SDL_CreateWindow failed");
	}

	m_window_id = SDL_GetWindowID(m_window);

	if (!cfg.fullscreen) {
		// Where window sizes are in pixels (Windows, X11), scale them by the display content scale.
		// Where they are in logical units (Wayland), the OS does it, and this ratio is 1.
		float scale = SDL_GetWindowDisplayScale(m_window) / SDL_GetWindowPixelDensity(m_window);
		if (scale > 0.0f && std::abs(scale - 1.0f) > 0.01f) {
			int width = int(std::lround(float(cfg.width) * scale));
			int height = int(std::lround(float(cfg.height) * scale));

			// Don't outgrow the display (sizes in the same units here)
			SDL_Rect bounds;
			if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(m_window), &bounds)) {
				width = std::min(width, bounds.w);
				height = std::min(height, bounds.h);
			}

			SDL_SetWindowSize(m_window, width, height);
			SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		}
	}

	SDL_ShowWindow(m_window);
	stopTextInput();
	useRegularCursor();

	Log::info("Created window {}x{} ({}x{} pixels), display scale {}", windowSize().width, windowSize().height,
		framebufferSize().width, framebufferSize().height, SDL_GetWindowDisplayScale(m_window));
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
	if (m_window) {
		SDL_DestroyWindow(m_window);
	}
}

bool Window::shouldClose() const
{
	return m_should_close;
}

void Window::pollEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		handleSdlEvent(event);
	}

	m_events.clear();
	std::swap(m_events, m_pending_events);
}

void Window::waitEvents()
{
	SDL_Event event;
	if (SDL_WaitEvent(&event)) {
		do {
			handleSdlEvent(event);
		} while (SDL_PollEvent(&event));
	}
}

Resolution Window::windowSize() const
{
	int width = 0, height = 0;
	SDL_GetWindowSize(m_window, &width, &height);
	return { width, height };
}

Resolution Window::framebufferSize() const
{
	int width = 0, height = 0;
	SDL_GetWindowSizeInPixels(m_window, &width, &height);
	return { width, height };
}

m::double2 Window::cursorPos() const
{
	float x = 0, y = 0;
	SDL_GetMouseState(&x, &y);
	return toFramebufferCoords(x, y);
}

bool Window::mouseButtonPressed(MouseButton button) const
{
	return (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(toSdlMouseButton(button))) != 0;
}

void Window::useRegularCursor()
{
	if (!SDL_SetWindowRelativeMouseMode(m_window, false)) {
		Log::warn("Can't disable relative mouse mode: {}", SDL_GetError());
	}
}

void Window::useGrabbedCursor()
{
	if (!SDL_SetWindowRelativeMouseMode(m_window, true)) {
		Log::warn("Can't enable relative mouse mode: {}", SDL_GetError());
	}
}

void Window::startTextInput()
{
	if (!SDL_StartTextInput(m_window)) {
		Log::warn("Can't start text input: {}", SDL_GetError());
	}
}

void Window::stopTextInput()
{
	SDL_StopTextInput(m_window);
}

bool Window::createVulkanSurface(VkInstance instance, VkSurfaceKHR *surface) const
{
	if (!SDL_Vulkan_CreateSurface(m_window, instance, nullptr, surface)) {
		Log::error("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
		return false;
	}

	return true;
}

std::span<const char *const> Window::requiredVulkanInstanceExtensions()
{
	Uint32 count = 0;
	const char *const *list = SDL_Vulkan_GetInstanceExtensions(&count);
	if (!list) {
		Log::error("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
		return {};
	}

	return { list, count };
}

bool Window::vulkanPresentationSupport(VkInstance instance, VkPhysicalDevice physical_device, uint32_t queue_family)
{
	return SDL_Vulkan_GetPresentationSupport(instance, physical_device, queue_family);
}

void Window::initLibrary()
{
	Log::info("Initializing SDL library");
	logSdlVersion();

	SDL_SetLogOutputFunction(&sdlLogCallback, nullptr);

#ifdef __linux__
	// SDL prefers X11 when the compositor lacks `wp_fifo_manager_v1`, we want Wayland anyway.
	// Environment variable `SDL_VIDEO_DRIVER` still takes precedence.
	SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
#endif
	// Don't turn SIGINT/SIGTERM into quit events, we handle (or not) them ourselves
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		Log::fatal("Can't init SDL: {}", SDL_GetError());
		throw Exception::fromError(VoxenErrc::ExternalLibFailure, "SDL init failed");
	}

	// Load Vulkan loader now, instance extensions are queried before window surfaces are created
	if (!SDL_Vulkan_LoadLibrary(nullptr)) {
		Log::fatal("Can't load Vulkan library: {}", SDL_GetError());
		SDL_Quit();
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "Vulkan library not found");
	}

	Log::info("SDL initialized successfully, video driver: {}", SDL_GetCurrentVideoDriver());
}

void Window::terminateLibrary() noexcept
{
	Log::info("Terminating SDL library");
	SDL_Vulkan_UnloadLibrary();
	SDL_Quit();
	SDL_SetLogOutputFunction(SDL_GetDefaultLogOutputFunction(), nullptr);
}

void Window::swap(Window &other) noexcept
{
	std::swap(m_window, other.m_window);
	std::swap(m_window_id, other.m_window_id);
	std::swap(m_should_close, other.m_should_close);
	std::swap(m_pending_events, other.m_pending_events);
	std::swap(m_events, other.m_events);
}

void Window::handleSdlEvent(const SDL_Event &event)
{
	switch (event.type) {
	case SDL_EVENT_QUIT:
		if (!m_should_close) {
			m_should_close = true;
			m_pending_events.emplace_back(WindowCloseEvent {});
		}
		return;

	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		if (event.window.windowID == m_window_id && !m_should_close) {
			m_should_close = true;
			m_pending_events.emplace_back(WindowCloseEvent {});
		}
		return;

	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		if (event.window.windowID == m_window_id) {
			m_pending_events.emplace_back(WindowResizeEvent {
				.framebuffer_size = { event.window.data1, event.window.data2 },
			});
		}
		return;

	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
		if (event.key.windowID == m_window_id) {
			m_pending_events.emplace_back(KeyEvent {
				.key = translateKey(event.key.scancode),
				.mods = translateMods(event.key.mod),
				.pressed = event.key.down,
				.repeat = event.key.repeat,
			});
		}
		return;

	case SDL_EVENT_TEXT_INPUT:
		if (event.text.windowID == m_window_id) {
			m_pending_events.emplace_back(TextInputEvent { .text = event.text.text });
		}
		return;

	case SDL_EVENT_TEXT_EDITING:
		if (event.edit.windowID == m_window_id) {
			m_pending_events.emplace_back(TextEditingEvent {
				.text = event.edit.text ? event.edit.text : "",
				.start = event.edit.start,
				.length = event.edit.length,
			});
		}
		return;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (MouseButton button;
			event.button.windowID == m_window_id && translateMouseButton(event.button.button, button)) {
			m_pending_events.emplace_back(MouseButtonEvent {
				.button = button,
				.mods = translateMods(SDL_GetModState()),
				.pressed = event.button.down,
				.clicks = event.button.down ? int32_t(event.button.clicks) : 0,
				.position = toFramebufferCoords(event.button.x, event.button.y),
			});
		}
		return;

	case SDL_EVENT_MOUSE_MOTION:
		if (event.motion.windowID == m_window_id) {
			m_pending_events.emplace_back(MouseMotionEvent {
				.position = toFramebufferCoords(event.motion.x, event.motion.y),
				.delta = toFramebufferCoords(event.motion.xrel, event.motion.yrel),
			});
		}
		return;

	case SDL_EVENT_MOUSE_WHEEL:
		if (event.wheel.windowID == m_window_id) {
			double sign = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0 : 1.0;
			m_pending_events.emplace_back(MouseWheelEvent {
				.delta = m::double2(event.wheel.x, event.wheel.y) * sign,
			});
		}
		return;

	default:
		return;
	}
}

m::double2 Window::toFramebufferCoords(float x, float y) const
{
	// SDL reports mouse coordinates in window (logical) units
	double density = SDL_GetWindowPixelDensity(m_window);
	return m::double2(x, y) * density;
}

} // namespace voxen::os
