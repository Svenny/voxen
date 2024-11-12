#include <voxen/gfx/vk/vk_swapchain.hpp>

#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/gfx/vk/vk_instance.hpp>
#include <voxen/gfx/vk/vk_utils.hpp>
#include <voxen/os/glfw_window.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <GLFW/glfw3.h>

#include <cassert>

namespace voxen::gfx::vk
{

Swapchain::Swapchain(Device &device, os::GlfwWindow &window) : m_device(device), m_window(window)
{
	if (!isCompatible(device)) {
		Log::error("Tried to create swapchain from device that can't present!");
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "device does not support present");
	}

	createPerFrame();
	defer_fail { destroyPerFrame(); };

	createSurface();
	defer_fail { device.instance().vkDestroySurface(m_surface); };

	recreateSwapchain();
	defer_fail { device.vkDestroySwapchain(m_swapchain); };
}

Swapchain::~Swapchain() noexcept
{
	if (imageAcquired()) {
		Log::warn("Destroying swapchain with acquired image, use-after-free might happen");
	}

	m_device.forceCompletion();

	destroySwapchain();
	destroySurface();
	destroyPerFrame();
}

void Swapchain::acquireImage()
{
	assert(m_num_images > 0);

	if (imageAcquired()) [[unlikely]] {
		Log::warn("Swapchain image already acquired! Only one can be acquired at a time");
		return;
	}

	m_device.waitForTimeline(Device::QueueMain, m_prev_usage_timelines[m_frame_index]);

	uint32_t acquired_index = NO_IMAGE_MARKER;

	// Vulkan spec for `vkAcquireNextImageKHR` says:
	//
	//     If an image is acquired successfully, vkAcquireNextImageKHR must either return VK_SUCCESS
	//     or VK_SUBOPTIMAL_KHR. The implementation may return VK_SUBOPTIMAL_KHR if the swapchain
	//     no longer matches the surface properties exactly, but can still be used for presentation.
	//
	//     ... Once vkAcquireNextImageKHR successfully acquires an image, the semaphore signal operation
	//     referenced by semaphore, if not VK_NULL_HANDLE, and the fence signal operation referenced by fence,
	//     if not VK_NULL_HANDLE, are submitted for execution. If vkAcquireNextImageKHR does not successfully
	//     acquire an image, semaphore and fence are unaffected.
	//
	// If we receive ERROR_SURFACE_LOST or ERROR_OUT_OF_DATE, recreate the necessary
	// objects and try acquiring one more time, using the same semaphore.
	// If that fails too, retry a few times, then fail and enter the bad state.
	VkResult res = m_device.dt().vkAcquireNextImageKHR(m_device.handle(), m_swapchain, UINT64_MAX,
		m_acquire_semaphores[m_frame_index], VK_NULL_HANDLE, &acquired_index);
	if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR) [[likely]] {
		// Ignore SUBOPTIMAL here, it will be handled during present
		m_image_index = acquired_index;
		return;
	}

	// Now, if anything fails, enter the bad state.
	defer_fail {
		m_device.forceCompletion();
		destroySwapchain();
		destroySurface();
		destroyPerFrame();
	};

	constexpr int RETRY_LIMIT = 3;
	int retry_count = 0;

	// During a fast resize sequence we might get OUT_OF_DATE immediately
	// with the new swapchain. If it persists after a few retries, then
	// something is likely screwed and we can't do much more to recover.
	while (retry_count < RETRY_LIMIT) {
		if (res == VK_ERROR_SURFACE_LOST_KHR) {
			Log::warn("Swapchain surface lost! Recreating");
			// Force completion as we're destroying objects immediately, not enqueueing (for simplicity)
			m_device.forceCompletion();
			// Destory the swapchain - it can't be reused as old anymore
			destroySwapchain();
			destroySurface();
			createSurface();
			recreateSwapchain();
		} else if (res == VK_ERROR_OUT_OF_DATE_KHR) {
			Log::info("Swapchain requires recreation, doing it");
			// Reuse the swapchain (`oldSwapchain` field), can even do this without stalling the GPU
			recreateSwapchain();
		} else {
			// We can't handle other error codes, fail and enter the bad state
			throw VulkanException(res, "vkAcquireNextImageKHR");
		}

		res = m_device.dt().vkAcquireNextImageKHR(m_device.handle(), m_swapchain, UINT64_MAX,
			m_acquire_semaphores[m_frame_index], VK_NULL_HANDLE, &acquired_index);
		if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR) {
			// Same as with the first attempt, SUBOPTIMAL will be handled during present
			m_image_index = acquired_index;
			return;
		}

		retry_count++;
		Log::info("Swapchain image acquire retry failed - {} ({}/{})", VulkanUtils::getVkResultString(res), retry_count,
			RETRY_LIMIT);
	}

	// We can't handle other error codes, fail and enter the bad state
	throw VulkanException(res, "vkAcquireNextImageKHR");
}

void Swapchain::presentImage(uint64_t timeline)
{
	assert(m_num_images > 0);
	assert(imageAcquired());

	// Remember the timeline to wait on it when
	m_prev_usage_timelines[m_frame_index] = timeline;

	// Vulkan spec for `vkQueuePresentKHR` says:
	//
	//	    Queueing an image for presentation defines a set of queue operations, including waiting on the semaphores
	//	    and submitting a presentation request to the presentation engine. However, the scope of this set of queue
	//     operations does not include the actual processing of the image by the presentation engine.
	//
	//     If vkQueuePresentKHR fails to enqueue the corresponding set of queue operations, it may return
	//     VK_ERROR_OUT_OF_HOST_MEMORY or VK_ERROR_OUT_OF_DEVICE_MEMORY. If it does, the implementation
	//     must ensure that the state and contents of any resources or synchronization primitives referenced
	//     is unaffected by the call or its failure.
	//
	//     If vkQueuePresentKHR fails in such a way that the implementation is unable to make that guarantee,
	//     the implementation must return VK_ERROR_DEVICE_LOST.
	//
	//     However, if the presentation request is rejected by the presentation engine with an error
	//     VK_ERROR_OUT_OF_DATE_KHR, VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT, or VK_ERROR_SURFACE_LOST_KHR,
	//     the set of queue operations are still considered to be enqueued and thus any semaphore wait operation
	//     specified in VkPresentInfoKHR will execute when the corresponding queue operation is complete.
	//
	//     vkQueuePresentKHR releases the acquisition of the images referenced by imageIndices. The queue family
	//     corresponding to the queue vkQueuePresentKHR is executed on must have ownership of the presented images
	//     as defined in Resource Sharing. vkQueuePresentKHR does not alter the queue family ownership, but
	//     the presented images must not be used again before they have been reacquired using vkAcquireNextImageKHR.
	//
	// I read it as the following table:
	//
	//               Return code                | Image acquisition | Semaphore wait op
	//     -------------------------------------+-------------------+-------------------
	//     VK_SUCCESS, VK_SUBOPTIMAL_KHR        |    Released       |     Enqueued
	//     VK_ERROR_OUT_OF_[HOST|DEVICE]_MEMORY |    Retained       |   Not enqueued
	//     VK_ERROR_OUT_OF_DATE_KHR or          |                   |
	//         VK_ERROR_SURFACE_LOST_KHR or     |    Released       |     Enqueued
	//         VK_ERROR_FULL_..._MODE_LOST_EXT  |                   |
	//     VK_ERROR_DEVICE_LOST                 |  Doesn't matter   |  Doesn't matter
	//
	// OUT_OF_[HOST|DEVICE]_MEMORY is essentially the same as DEVICE_LOST for us. We won't pretend
	// we can meaningfully handle it, and will simply destroy everything, entering the bad state.
	//
	// With other return codes the image is released and semaphore wait operation is enqueued.
	// Therefore we can forget about image acquisition and advance the frame index right away.
	uint32_t image_index = std::exchange(m_image_index, NO_IMAGE_MARKER);
	uint32_t frame_index = std::exchange(m_frame_index, (m_frame_index + 1) % MAX_FRAME_LAG);

	VkPresentInfoKHR present_info {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_present_semaphores[frame_index],
		.swapchainCount = 1,
		.pSwapchains = &m_swapchain,
		.pImageIndices = &image_index,
		.pResults = nullptr,
	};

	VkResult res = m_device.dt().vkQueuePresentKHR(m_device.mainQueue(), &present_info);
	if (res == VK_SUCCESS) [[likely]] {
		return;
	}

	// Now, if anything fails, enter the bad state.
	// This includes swapchain recreation failure - don't retry it.
	defer_fail {
		m_device.forceCompletion();
		destroySwapchain();
		destroySurface();
		destroyPerFrame();
	};

	if (res == VK_ERROR_SURFACE_LOST_KHR) {
		Log::warn("Swapchain surface lost! Recreating");
		// Force completion as we're destroying objects immediately, not enqueueing (for simplicity)
		m_device.forceCompletion();
		// Destory the swapchain - it can't be reused as old anymore
		destroySwapchain();
		destroySurface();
		createSurface();
		recreateSwapchain();
		// We did not actually present (this image is lost)
		// but the current state is OK, we can acquire again
		return;
	}

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		Log::info("Swapchain requires recreation ({}), doing it", VulkanUtils::getVkResultString(res));
		// Reuse the swapchain (`oldSwapchain` field), can even do this without stalling the GPU
		recreateSwapchain();
		// We either did (SUBOPTIMAL) or did not (ERROR_OUT_OF_DATE)
		// present but the current state is OK, we can acquire again
		return;
	}

	// We can't handle other error codes, fail and enter the bad state
	throw VulkanException(res, "vkQueuePresentKHR");
}

VkImage Swapchain::currentImage()
{
	assert(imageAcquired());
	return m_images[m_image_index];
}

VkImageView Swapchain::currentImageRtv()
{
	assert(imageAcquired());
	return m_image_rtvs[m_image_index];
}

VkSemaphore Swapchain::currentAcquireSemaphore()
{
	assert(imageAcquired());
	return m_acquire_semaphores[m_frame_index];
}

VkSemaphore Swapchain::currentPresentSemaphore()
{
	assert(imageAcquired());
	return m_present_semaphores[m_frame_index];
}

bool Swapchain::isCompatible(Device &device)
{
	VkInstance instance = device.instance().handle();
	VkPhysicalDevice phys_dev = device.physicalDevice().handle();

	return glfwGetPhysicalDevicePresentationSupport(instance, phys_dev, device.info().main_queue_family) == GLFW_TRUE;
}

void Swapchain::createPerFrame()
{
	for (uint32_t i = 0; i < MAX_FRAME_LAG; i++) {
		VkSemaphoreCreateInfo semaphore_info {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
		};

		char buf[64];

		snprintf(buf, std::size(buf), "swapchain/sema/acquire@%u", i);
		m_acquire_semaphores[i] = m_device.vkCreateSemaphore(semaphore_info, buf);

		snprintf(buf, std::size(buf), "swapchain/sema/present@%u", i);
		m_present_semaphores[i] = m_device.vkCreateSemaphore(semaphore_info, buf);
	}
}

void Swapchain::createSurface()
{
	assert(m_surface == VK_NULL_HANDLE);

	VkResult res = glfwCreateWindowSurface(m_device.instance().handle(), m_window.glfwHandle(), nullptr, &m_surface);
	if (res != VK_SUCCESS) {
		Log::error("Window surface creation failed - {}", VulkanUtils::getVkResultString(res));
		throw VulkanException(res, "glfwCreateWindowSurface");
	}
	// Destroy the newly created surface immediately if a next step fails
	defer_fail { destroySurface(); };

	updateSwapchainParameters();
}

uint32_t Swapchain::updateSwapchainParameters()
{
	VkPhysicalDevice phys_dev = m_device.physicalDevice().handle();
	auto &instance = m_device.instance();

	// Ensure the surface has the required capabilities. Should pass on any driver.
	VkSurfaceCapabilitiesKHR caps = instance.vkGetPhysicalDeviceSurfaceCapabilities(phys_dev, m_surface);

	if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
		Log::error("Window surface doesn't support render target usage (what?!)");
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "surface doesn't support render target usage");
	}
	if (!(caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)) {
		Log::error("Window surface doesn't support identity transform (what?!)");
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "surface doesn't support identity transform");
	}
	if (!(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) {
		Log::error("Window surface doesn't support opaque composite alpha (what?!)");
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "surface doesn't support opaque composite alpha");
	}

	// Select image count
	if (caps.minImageCount > MAX_IMAGES) {
		Log::error("Too many swapchain images needed - {}, our limit is {}", caps.minImageCount, MAX_IMAGES);
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "too many swapchain images needed");
	}
	if (caps.maxImageCount != 0 && caps.maxImageCount < MAX_FRAME_LAG) {
		Log::error("Too few swapchain images supported - {}, we need at least {}", caps.maxImageCount, MAX_FRAME_LAG);
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "too few swapchain images supported");
	}

	uint32_t num_images = std::max(caps.minImageCount, MAX_FRAME_LAG);

	// Select image extent
	if (caps.currentExtent.width != 0xFFFFFFFF && caps.currentExtent.height != 0xFFFFFFFF) {
		m_image_extent = caps.currentExtent;
	} else {
		m_image_extent = {};
	}

	// `currentExtent` is zero or allows any size?
	if (m_image_extent.width == 0 || m_image_extent.height == 0) {
		std::tie(m_image_extent.width, m_image_extent.height) = m_window.framebufferSize();
	}

	// Still zero?
	if (m_image_extent.width == 0 || m_image_extent.height == 0) {
		Log::info("Window surface size is (0, 0) - minimized? Waiting for resize");
		std::tie(m_image_extent.width, m_image_extent.height) = m_window.waitUntilUnMinimized();
	}

	// Select image format
	extras::dyn_array<VkSurfaceFormatKHR> formats = instance.vkGetPhysicalDeviceSurfaceFormats(phys_dev, m_surface);
	// TODO: configurable format selection (HDR?)
	bool format_found = false;
	for (auto &fmt : formats) {
		if (fmt.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			continue;
		}

		if (fmt.format == VK_FORMAT_R8G8B8A8_SRGB || fmt.format == VK_FORMAT_B8G8R8A8_SRGB) {
			m_image_format = fmt.format;
			m_image_color_space = fmt.colorSpace;
			format_found = true;
			break;
		}
	}

	if (!format_found) {
		Log::error("Window surface doesn't support 8-bit sRGB format");
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "8-bit sRGB surface format unsupported");
	}

	// Select image present mode
	extras::dyn_array<VkPresentModeKHR> present_modes = instance.vkGetPhysicalDeviceSurfacePresentModes(phys_dev,
		m_surface);
	// TODO: configurable present mode selection
	auto iter = std::ranges::find(present_modes, VK_PRESENT_MODE_FIFO_KHR);
	if (iter == present_modes.end()) {
		Log::error("Window surface doesn't support FIFO present mode");
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "FIFO surface present mode unsupported");
	}
	m_present_mode = *iter;

	return num_images;
}

void Swapchain::recreateSwapchain()
{
	assert(!imageAcquired());

	uint32_t new_num_images = updateSwapchainParameters();

	// Old swapchain, if present, becomes retired regardless of
	// the new one creation result, so queue it for destruction
	// unconditionally. Images/RTVs can be queued immediately,
	// swapchain handle is needed for the call, defer it.
	for (uint32_t i = 0; i < m_num_images; i++) {
		m_device.enqueueDestroy(m_image_rtvs[i]);
		m_image_rtvs[i] = VK_NULL_HANDLE;
		// Image was not created by us
		m_images[i] = VK_NULL_HANDLE;
	}
	m_num_images = 0;

	VkSwapchainKHR old_swapchain = std::exchange(m_swapchain, VK_NULL_HANDLE);
	defer { m_device.enqueueDestroy(old_swapchain); };

	Log::info("Creating swapchain with {} images, resolution {}x{}", new_num_images, m_image_extent.width,
		m_image_extent.height);

	VkSwapchainCreateInfoKHR create_info {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = m_surface,
		.minImageCount = new_num_images,
		.imageFormat = m_image_format,
		.imageColorSpace = m_image_color_space,
		.imageExtent = m_image_extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = m_present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = old_swapchain,
	};

	m_swapchain = m_device.vkCreateSwapchain(create_info);
	// Destroy the newly created swapchain immediately if we can't get images/RTVs
	defer_fail { destroySwapchain(); };

	uint32_t num_images = MAX_IMAGES;
	VkResult res = m_device.dt().vkGetSwapchainImagesKHR(m_device.handle(), m_swapchain, &num_images, m_images);
	if (res == VK_INCOMPLETE) {
		Log::error("Too many swapchain images created - {}, our limit is {}", num_images, MAX_IMAGES);
		throw Exception::fromError(VoxenErrc::GfxFailure, "too many swapchain images created");
	}
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkGetSwapchainImagesKHR");
	}

	m_num_images = num_images;

	for (uint32_t i = 0; i < num_images; i++) {
		VkImageViewUsageCreateInfo rtv_usage_info {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
			.pNext = nullptr,
			.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		};

		VkImageViewCreateInfo rtv_info {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = &rtv_usage_info,
			.flags = 0,
			.image = m_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = m_image_format,
			.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY },
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
		};

		char buf[64];

		snprintf(buf, std::size(buf), "swapchain/img/%u", i);
		m_device.setObjectName(m_images[i], buf);

		snprintf(buf, std::size(buf), "swapchain/img/%u/rtv", i);
		m_image_rtvs[i] = m_device.vkCreateImageView(rtv_info, buf);
	}
}

void Swapchain::destroyPerFrame() noexcept
{
	for (uint32_t i = 0; i < MAX_FRAME_LAG; i++) {
		m_device.vkDestroySemaphore(m_acquire_semaphores[i]);
		m_acquire_semaphores[i] = VK_NULL_HANDLE;

		m_device.vkDestroySemaphore(m_present_semaphores[i]);
		m_present_semaphores[i] = VK_NULL_HANDLE;
	}
}

void Swapchain::destroySurface() noexcept
{
	m_device.instance().vkDestroySurface(m_surface);
	m_surface = VK_NULL_HANDLE;

	// Clear surface information, just in case
	m_image_format = VK_FORMAT_UNDEFINED;
	m_image_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	m_image_extent = {};
	m_present_mode = VK_PRESENT_MODE_FIFO_KHR;
}

void Swapchain::destroySwapchain() noexcept
{
	for (uint32_t i = 0; i < m_num_images; i++) {
		m_device.vkDestroyImageView(m_image_rtvs[i]);
		m_image_rtvs[i] = VK_NULL_HANDLE;
		// Image was not created by us
		m_images[i] = VK_NULL_HANDLE;
	}
	m_num_images = 0;

	m_device.vkDestroySwapchain(m_swapchain);
	m_swapchain = VK_NULL_HANDLE;
}

} // namespace voxen::gfx::vk
