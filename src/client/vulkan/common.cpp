#include <voxen/client/vulkan/common.hpp>
#include <voxen/util/log.hpp>

#include <malloc.h>

namespace voxen
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
	case VK_INCOMPLETE: return "VK_INCOMPLETE";
	case VK_EVENT_RESET: return "VK_EVENT_RESET";
	case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
	// Error codes
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
		// TODO: these are not all codes, add the rest
	default: return "[UNKNOWN]";
	}
}

const char *getVkResultDescription(VkResult result) noexcept {
	// Copy-pasted from Vulkan spec 1.2.132
	switch (result) {
	// Result codes
	case VK_SUCCESS: return "Command successfully completed";
	case VK_NOT_READY: return "A fence or query has not yet completed";
		// TODO: these are not all codes, add the rest
	// Error codes
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "A host memory allocation has failed";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "A device memory allocation has failed";
	case VK_ERROR_INITIALIZATION_FAILED:
		return "Initialization of an object could not be completed for implementation-specific reasons";
	case VK_ERROR_DEVICE_LOST: return "The logical or physical device has been lost";
	case VK_ERROR_MEMORY_MAP_FAILED: return "Mapping of a memory object has failed";
		// TODO: these are not all codes, add the rest
	default: return "Unknown or wrong VkResult value";
	}
}

}
