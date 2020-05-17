#include <voxen/client/vulkan/common.hpp>
#include <voxen/util/log.hpp>

#include <malloc.h>

namespace voxen::client
{

VulkanException::VulkanException(VkResult result, const std::experimental::source_location &loc)
   : Exception(loc), m_result(result) {
	m_message = "Vulkan error: ";
	m_message += getVkResultString(result);
	m_message += " (";
	m_message += getVkResultDescription(result);
	m_message += ")";
}

static void *VKAPI_PTR vulkanMalloc(void *user_data, size_t size, size_t align,
                                    VkSystemAllocationScope scope) noexcept {
	(void)scope;
	void *ptr = aligned_alloc(align, size);
	if (!ptr) {
		Log::warn("Vulkan code has ran out of memory!");
		return nullptr;
	}
	if (user_data) {
		size_t sz = malloc_usable_size(ptr);
		std::atomic_size_t *allocated = reinterpret_cast<std::atomic_size_t *>(user_data);
		allocated->fetch_add(sz);
	}
	return ptr;
}

static void VKAPI_PTR vulkanFree(void *user_data, void *ptr) noexcept {
	if (user_data) {
		size_t sz = malloc_usable_size(ptr);
		std::atomic_size_t *allocated = reinterpret_cast<std::atomic_size_t *>(user_data);
		allocated->fetch_sub(sz);
	}
	free(ptr);
}

static void *VKAPI_PTR vulkanRealloc(void *user_data, void *original, size_t size, size_t align,
                                     VkSystemAllocationScope scope) noexcept {
	(void)scope;
	if (size == 0) {
		vulkanFree(user_data, original);
		return nullptr;
	}
	// `realloc` does not guarantee preserving alignment from `aligned_alloc`.
	// We need to either implement some kind of `aligned_realloc` or to just resort
	// to `aligned_alloc + memcpy + free` scheme.
	void *ptr = aligned_alloc(align, size);
	if (!ptr) {
		Log::warn("Vulkan code has ran out of memory!");
		return nullptr;
	}
	size_t old_sz = malloc_usable_size(original);
	size_t new_sz = malloc_usable_size(ptr);
	if (original)
		memcpy(ptr, original, std::min(old_sz, new_sz));
	if (user_data) {
		std::atomic_size_t *allocated = reinterpret_cast<std::atomic_size_t *>(user_data);
		allocated->fetch_add(new_sz - old_sz);
	}
	return ptr;
}

VulkanHostAllocator VulkanHostAllocator::g_instance;

VulkanHostAllocator::VulkanHostAllocator() : m_allocated(0) {
	m_callbacks.pUserData = &m_allocated;
	m_callbacks.pfnAllocation = vulkanMalloc;
	m_callbacks.pfnReallocation = vulkanRealloc;
	m_callbacks.pfnFree = vulkanFree;
	m_callbacks.pfnInternalAllocation = nullptr;
	m_callbacks.pfnInternalFree = nullptr;
}

VulkanHostAllocator::~VulkanHostAllocator() {
	size_t leftover = m_allocated.load();
	if (leftover != 0)
		Log::warn("Vulkan code has memory leak! Approx. {} bytes left over", leftover);
}

const char *getVkResultString(VkResult result) noexcept {
	// Copy-pasted from Vulkan spec 1.2.132
	switch (result) {
	// Result codes
	case VK_SUCCESS: return "VK_SUCCESS";
	case VK_NOT_READY: return "VK_NOT_READY";
	case VK_TIMEOUT: return "VK_TIMEOUT";
	case VK_EVENT_SET: return "VK_EVENT_SET";
	case VK_EVENT_RESET: return "VK_EVENT_RESET";
	case VK_INCOMPLETE: return "VK_INCOMPLETE";
	case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
	// Error codes
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
	case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
	case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
	case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
	case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
	case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
	case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
		return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
	case VK_ERROR_NOT_PERMITTED_EXT: return "VK_ERROR_NOT_PERMITTED_EXT";
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
	default: return "[UNKNOWN]";
	}
}

const char *getVkResultDescription(VkResult result) noexcept {
	// Copy-pasted from Vulkan spec 1.2.132
	switch (result) {
	// Result codes
	case VK_SUCCESS: return "Command successfully completed";
	case VK_NOT_READY: return "A fence or query has not yet completed";
	case VK_TIMEOUT: return "A wait operation has not completed in the specified time";
	case VK_EVENT_SET: return "An event is signaled";
	case VK_EVENT_RESET: return "An event is unsignaled";
	case VK_INCOMPLETE: return "A return array was too small for the result";
	case VK_SUBOPTIMAL_KHR:
		return "A swapchain no longer matches the surface properties exactly, "
		       "but can still be used to present to the surface successfully";
	// Error codes
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "A host memory allocation has failed";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "A device memory allocation has failed";
	case VK_ERROR_INITIALIZATION_FAILED:
		return "Initialization of an object could not be completed for implementation-specific reasons";
	case VK_ERROR_DEVICE_LOST: return "The logical or physical device has been lost";
	case VK_ERROR_MEMORY_MAP_FAILED: return "Mapping of a memory object has failed";
	case VK_ERROR_LAYER_NOT_PRESENT: return "A requested layer is not present or could not be loaded";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "A requested extension is not supported";
	case VK_ERROR_FEATURE_NOT_PRESENT: return "A requested feature is not supported";
	case VK_ERROR_INCOMPATIBLE_DRIVER:
		return "The requested version of Vulkan is not supported by the driver "
		       "or is otherwise incompatible for implementation-specific reasons";
	case VK_ERROR_TOO_MANY_OBJECTS: return "Too many objects of the type have already been created";
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "A requested format is not supported on this device";
	case VK_ERROR_FRAGMENTED_POOL: return "A pool allocation has failed due to fragmentation of the pool’s memory";
	case VK_ERROR_UNKNOWN:
		return "An unknown error has occurred; either the application "
		       "has provided invalid input, or an implementation failure has occurred";
	case VK_ERROR_OUT_OF_POOL_MEMORY: return "A pool memory allocation has failed";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "An external handle is not a valid handle of the specified type";
	case VK_ERROR_FRAGMENTATION: return "A descriptor pool creation has failed due to fragmentation";
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
		return "A buffer creation or memory allocation failed because the requested address is not available";
	case VK_ERROR_SURFACE_LOST_KHR: return "A surface is no longer available";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
		return "The requested window is already in use by Vulkan "
		       "or another API in a manner which prevents it from being used again";
	case VK_ERROR_OUT_OF_DATE_KHR:
		return "A surface has changed in such a way that it is no longer compatible with "
		       "the swapchain, and further presentation requests using the swapchain will fail";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
		return "The display used by a swapchain does not use the same presentable "
		       "image layout, or is incompatible in a way that prevents sharing an image";
	case VK_ERROR_VALIDATION_FAILED_EXT: return "No description in spec";
	case VK_ERROR_INVALID_SHADER_NV: return "One or more shaders failed to compile or link";
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
		return "No description in spec";
	case VK_ERROR_NOT_PERMITTED_EXT: return "The caller does not have sufficient privileges";
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
		return "An operation on a swapchain created with VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT "
		       "failed as it did not have exlusive full-screen access";
	default: return "Unknown or wrong VkResult value";
	}
}

}
