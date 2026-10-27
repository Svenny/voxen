#pragma once

#include <voxen/gfx/gfx_consts.hpp>
#include <voxen/gfx/vk/vk_include.hpp>
#include <voxen/os/os_fwd.hpp>

#include <cstdint>
#include <vector>

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

	Swapchain(Device &device, os::Window &window);
	Swapchain(Swapchain &&) = delete;
	Swapchain(const Swapchain &) = delete;
	Swapchain &operator=(Swapchain &&) = delete;
	Swapchain &operator=(const Swapchain &) = delete;
	~Swapchain() noexcept;

	// Attempt to acquire a new frame from the swapchain.
	//
	// There can be only one frame acquired at a time, calling `acquireImage()`
	// twice before `presentImage()` will log warning and do nothing more,
	// the already acquired image will remain unchanged. After `abandonImage()`
	// this returns the abandoned image again, without any warning.
	//
	// Usually this function will recover from the underlying WSI problems
	// (like window resize with the swapchain going out of date) transparently.
	//
	// While the window is minimized and the surface has zero size (Win32), this function blocks
	// until the window is restored. If the window is closed meanwhile, no image is acquired.
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
	// Give up rendering to the acquired image, e.g. when an exception interrupts the frame
	// before its submission. Call it only if no wait on `currentAcquireSemaphore()` was submitted.
	//
	// Submits a wait on the acquire semaphore to the main device queue (Vulkan doesn't allow
	// destroying or reusing a binary semaphore with a pending signal) and keeps the image
	// acquired: Vulkan can't release it without presenting. The next `acquireImage()` returns
	// the same image, then `currentAcquireSemaphore()` is null, as the wait is already done.
	// Destroying the swapchain is also safe after this call. Does nothing if no image is
	// acquired. Doesn't throw: if the submission fails, the image stays as it was.
	void abandonImage() noexcept;

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
	// Null if the image was abandoned and reacquired: then it's already waited
	// by an earlier main queue submission, there is nothing to wait for.
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
	os::Window &m_window;

	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

	VkFormat m_image_format = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR m_image_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkExtent2D m_image_extent = {};
	// The surface leaves image extent to us (Wayland), we take it from the window
	bool m_extent_from_window = false;
	VkPresentModeKHR m_present_mode = VK_PRESENT_MODE_FIFO_KHR;

	uint32_t m_num_images = 0;
	uint32_t m_image_index = NO_IMAGE_MARKER;
	// Set by `abandonImage()`: the acquire semaphore of the current image is already waited
	bool m_image_abandoned = false;
	VkImage m_images[MAX_IMAGES] = {};
	VkImageView m_image_rtvs[MAX_IMAGES] = {};

	// Signaled by `vkAcquireNextImageKHR` and waited by the rendering submission. Indexed by
	// frame index: it can be reused once that submission is complete (timeline is reached).
	VkSemaphore m_acquire_semaphores[gfx::Consts::MAX_PENDING_FRAMES] = {};
	// Signaled by the rendering submission and waited by `vkQueuePresentKHR`. Indexed by
	// image index: GPU timeline does not cover present operations, the only signal that
	// a present waiting on this semaphore has completed is re-acquiring the same image.
	// See https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
	VkSemaphore m_present_semaphores[MAX_IMAGES] = {};
	// Bit N is set if image N of the current swapchain has been presented at least once
	uint32_t m_presented_images_mask = 0;

	// Swapchain replaced by recreation, with present semaphores of its images.
	// Its images might never be acquired again, so we can't know when its presents
	// complete (without VK_KHR_swapchain_maintenance1). Presents are processed in order,
	// so destroy it once any present of a newer swapchain is known to be complete.
	struct RetiredSwapchain {
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkSemaphore present_semaphores[MAX_IMAGES] = {};
	};

	std::vector<RetiredSwapchain> m_retired_swapchains;

	uint32_t m_frame_index = 0;
	uint64_t m_prev_usage_timelines[gfx::Consts::MAX_PENDING_FRAMES] = {};

	void createPerFrame();
	void createSurface();
	uint32_t updateSwapchainParameters();
	void recreateSwapchain();
	void destroyPerFrame() noexcept;
	void destroySurface() noexcept;
	// Destroys the current swapchain only
	void destroyCurrentSwapchain() noexcept;
	// Destroys the current and all retired swapchains, GPU must be idle
	void destroySwapchain() noexcept;
	// Enqueue destruction of all retired swapchains (when it's known to be safe)
	void releaseRetiredSwapchains() noexcept;
};

} // namespace voxen::gfx::vk
