#pragma once

#include <voxen/gfx/vk/vk_include.hpp>

namespace voxen::os
{

class GlfwWindow;

}

namespace voxen::gfx::vk
{

class Device;

// Manages the whole Vulkan WSI - VkSurface and VkSwapchain, format selection,
// frame rate throttling etc. Supported features:
// - One image can be acquired at a time
// - Simple synchronization with rendering
// - Guaranteed frame rate throttling (currently not configurable)
// - Automatic swapchain/surface recreation, e.g. when resizing
//
// TODO:
// - Format selection, HDR (currently fixed RGBA/BGRA8 sRGB)
// - Present mode selection/switching, VSync on/off (currently fixed FIFO)
// - Configurable frame rate throttling - double or triple buffering
// - Target frame rate control? (like limit to 60 FPS on 120 FPS display)
// - Frame rate statistics?
// - Presents from compute queue (currently only main), UAV image usage
class Swapchain {
public:
	// The maximal supported number of swapchain images.
	// The actual used number will depend on the underlying driver.
	// If the underlying Vulkan swapchain allocates more images, creation will fail.
	// This is not directly related to the presentation latency.
	constexpr static uint32_t MAX_IMAGES = 4;
	// The maximal number of frames in flight (both CPU and GPU workloads).
	// Independent of swapchain image count, fixed at creation time.
	// This is directly related to the presentation latency.
	constexpr static uint32_t MAX_FRAME_LAG = 3;

	Swapchain(Device &device, os::GlfwWindow &window);
	Swapchain(Swapchain &&) = delete;
	Swapchain(const Swapchain &) = delete;
	Swapchain &operator=(Swapchain &&) = delete;
	Swapchain &operator=(const Swapchain &) = delete;
	~Swapchain() noexcept;

	// Attempt to acquire a new frame from the swapchain.
	//
	// There can be only one frame acquired at a time, calling `acquireImage()`
	// twice before `presentImage()` will log warning and do nothing more,
	// the already acquired image will remain unchanged.
	//
	// Usually this function will recover from the underlying WSI problems
	// (like window resize with the swapchain going out of date) transparently.
	//
	// NOTE: this may lead to changes in image format/resolution/etc.
	// Values returned by `imageFormat()`, `imageColorSpace()`,
	// `imageExtent()` and `presentMode()` can change after this call, make
	// sure to check and reconfigure your rendering pipeline accordingly.
	//
	// However, some (very unlikely to happen) errors are unrecoverable:
	// this function will throw an exception, and the object will enter "bad state".
	// No objects are leaked, but using the swapchain is no longer possible,
	// all operations and queries will have undefined behavior after that.
	void acquireImage();
	// Queue the previously acquired image for presentation.
	//
	// Presentations happen on the main device queue, and the following set
	// of operations MUST be enqueued before calling this function:
	// 1. Wait on `currentAcquireSemaphore()` before rendering to `currentImage()`
	// 2. Signal `currentPresentSemaphore()` after rendering to `currentImage()`
	// 3. `timeline` must be assigned to the last rendering submission,
	//    and it must belong to the main device queue.
	//
	// This call releases the image acquisition. Calling it twice
	// before `acquireImage()` will trigger undefined behavior.
	//
	// The same transparent problem handling and unrecoverable error rules apply
	// as with `acquireImage()`. If this function throws, the object is broken
	// and can only be destroyed, all operations will have undefined behavior.
	void presentImage(uint64_t timeline);

	// Currently acquired image handle.
	// This is a 2D image with one MIP level and one array layer, the only
	// enabled usage bit is `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT`.
	// Its format is `imageFormat()`, resolution is `imageExtent()`.
	// Undefined when no image is acquired.
	VkImage currentImage();
	// Image view of the currently acquired image. The only usage bit enabled
	// for it is `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT` - hence it's called RTV.
	// There is no format reinterpretation - view is in `imageFormat()`.
	// Undefined when no image is acquired.
	VkImageView currentImageRtv();
	// Binary semaphore with enqueued signal operation that will complete
	// when `currentImage()` is safe to render to. You MUST submit exactly
	// one wait operation on this semaphore to the main device queue
	// before calling `presentImage()`, or an evil sync UB will happen.
	// Undefined when no image is acquired.
	VkSemaphore currentAcquireSemaphore();
	// Binary semaphore in unsignaled state that must be signaled when
	// `currentImage()` is completely rendered and is safe to present.
	// You MUST submit exactly one signal operation on this semaphore to the main
	// device queue before calling `presentImage()`, or an evil sync UB will happen.
	// Undefined when no image is acquired.
	VkSemaphore currentPresentSemaphore();

	VkFormat imageFormat() const noexcept { return m_image_format; }
	VkColorSpaceKHR imageColorSpace() const noexcept { return m_image_color_space; }
	VkExtent2D imageExtent() const noexcept { return m_image_extent; }
	VkPresentModeKHR presentMode() const noexcept { return m_present_mode; }

	// Returns `true` if there is an image currently acquired.
	// Then `current*` family of functions will return defined handles.
	bool imageAcquired() const noexcept { return m_image_index != NO_IMAGE_MARKER; }
	// Returns `true` if the swapchain is in "bad state", which can only
	// (and always will) happen if `acquireImage()` or `presentImage()` fails.
	// In this state, the object can only be destroyed;
	// all other operations have undefined behavior.
	bool badState() const noexcept { return m_surface == VK_NULL_HANDLE; }

	// Checks whether this device can present from the main queue
	static bool isCompatible(Device &device);

private:
	constexpr static uint32_t NO_IMAGE_MARKER = UINT32_MAX;

	Device &m_device;
	os::GlfwWindow &m_window;

	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

	VkFormat m_image_format = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR m_image_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkExtent2D m_image_extent = {};
	VkPresentModeKHR m_present_mode = VK_PRESENT_MODE_FIFO_KHR;

	uint32_t m_num_images = 0;
	uint32_t m_image_index = NO_IMAGE_MARKER;
	VkImage m_images[MAX_IMAGES] = {};
	VkImageView m_image_rtvs[MAX_IMAGES] = {};

	uint32_t m_frame_index = 0;
	VkSemaphore m_acquire_semaphores[MAX_FRAME_LAG] = {};
	VkSemaphore m_present_semaphores[MAX_FRAME_LAG] = {};
	uint64_t m_prev_usage_timelines[MAX_FRAME_LAG] = {};

	void createPerFrame();
	void createSurface();
	uint32_t updateSwapchainParameters();
	void recreateSwapchain();
	void destroyPerFrame() noexcept;
	void destroySurface() noexcept;
	void destroySwapchain() noexcept;
};

} // namespace voxen::gfx::vk
