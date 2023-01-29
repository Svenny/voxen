#include <voxen/client/vulkan/common.hpp>

#include <voxen/client/vulkan/config.hpp>
#include <voxen/util/log.hpp>

#include <cstddef>
#include <malloc.h>

namespace voxen::client::vulkan
{

uint32_t VulkanUtils::alignUp(uint32_t size, uint32_t alignment) noexcept
{
	return (size + alignment - 1u) & ~(alignment - 1u);
}

uint64_t VulkanUtils::alignUp(uint64_t size, uint64_t alignment) noexcept
{
	return (size + alignment - 1u) & ~(alignment - 1u);
}

uint64_t VulkanUtils::calcFraction(uint64_t size, uint64_t numerator, uint64_t denomenator) noexcept
{
	return (size * numerator + denomenator - 1u) / denomenator;
}

VulkanException::VulkanException(VkResult result, const char *api, extras::source_location loc) noexcept
	: Exception(loc), m_result(result)
{
	try {
		std::string_view err = VulkanUtils::getVkResultString(result);
		if (api) {
			Log::error("{} failed with error code {}", api, err, loc);
			m_message = fmt::format("{} failed: {}", api, err);
		} else {
			Log::error("Vulkan API call failed with error code {}", err, loc);
			m_message = fmt::format("Vulkan error: {}", err);
		}
	} catch (...) {
		m_exception_occured = true;
	}
}

const char *VulkanException::what() const noexcept
{
	constexpr static char EXCEPTION_OCCURED_MSG[] =
		"Exception occured during creating VulkanException, message is lost";

	if (m_exception_occured) {
		return EXCEPTION_OCCURED_MSG;
	}

	return m_message.c_str();
}

static void *VKAPI_PTR vulkanMalloc(void *user_data, size_t size, size_t align,
                                    VkSystemAllocationScope /*scope*/) noexcept
{
	void *ptr = aligned_alloc(align, VulkanUtils::alignUp(size, align));
	if (!ptr) {
		Log::warn("Vulkan code has ran out of memory!");
		return nullptr;
	}
	if (user_data) {
		size_t sz = malloc_usable_size(ptr);
		auto allocated = reinterpret_cast<std::atomic_size_t *>(user_data);
		allocated->fetch_add(sz);
	}
	return ptr;
}

static void VKAPI_PTR vulkanFree(void *user_data, void *ptr) noexcept
{
	if (user_data) {
		size_t sz = malloc_usable_size(ptr);
		auto allocated = reinterpret_cast<std::atomic_size_t *>(user_data);
		allocated->fetch_sub(sz);
	}
	free(ptr);
}

static void *VKAPI_PTR vulkanRealloc(void *user_data, void *original, size_t size, size_t align,
                                     VkSystemAllocationScope /*scope*/) noexcept
{
	if (size == 0) {
		vulkanFree(user_data, original);
		return nullptr;
	}

	size_t old_sz = malloc_usable_size(original);
	void *new_ptr;

	if (align <= alignof(std::max_align_t)) {
		// We can rely on basic alignment guarantee of `realloc`
		new_ptr = realloc(original, size);
	} else {
		// We have to resort to `aligned_alloc+memcpy+free` scheme
		new_ptr = aligned_alloc(align, VulkanUtils::alignUp(size, align));
		if (new_ptr) {
			memcpy(new_ptr, original, std::min(old_sz, size));
			free(original);
		}
	}

	if (!new_ptr) {
		Log::warn("Vulkan code has ran out of memory!");
		return nullptr;
	}

	size_t new_sz = malloc_usable_size(new_ptr);
	if (user_data) {
		auto allocated = reinterpret_cast<std::atomic_size_t *>(user_data);
		allocated->fetch_add(new_sz - old_sz);
	}
	return new_ptr;
}

HostAllocator HostAllocator::g_instance;

HostAllocator::HostAllocator() noexcept : m_allocated(0)
{
	m_callbacks.pUserData = &m_allocated;
	m_callbacks.pfnAllocation = vulkanMalloc;
	m_callbacks.pfnReallocation = vulkanRealloc;
	m_callbacks.pfnFree = vulkanFree;
	m_callbacks.pfnInternalAllocation = nullptr;
	m_callbacks.pfnInternalFree = nullptr;
}

HostAllocator::~HostAllocator() noexcept
{
	size_t leftover = m_allocated.load();
	if (leftover != 0) {
		Log::warn("Vulkan code has memory leak! Approx. {} bytes left over", leftover);
	}
}

const VkAllocationCallbacks *HostAllocator::callbacks() noexcept
{
	if constexpr (Config::TRACK_HOST_ALLOCATIONS) {
		return &g_instance.m_callbacks;
	}

	return nullptr;
}

std::string_view VulkanUtils::getVkResultString(VkResult result) noexcept
{
	using namespace std::string_view_literals;

#define CASE(err) case err: return #err##sv
	switch (result) {
	// Result codes
	CASE(VK_SUCCESS);
	CASE(VK_NOT_READY);
	CASE(VK_TIMEOUT);
	CASE(VK_EVENT_SET);
	CASE(VK_EVENT_RESET);
	CASE(VK_INCOMPLETE);
	CASE(VK_SUBOPTIMAL_KHR);
	CASE(VK_THREAD_IDLE_KHR);
	CASE(VK_THREAD_DONE_KHR);
	CASE(VK_OPERATION_DEFERRED_KHR);
	CASE(VK_OPERATION_NOT_DEFERRED_KHR);
	CASE(VK_PIPELINE_COMPILE_REQUIRED);
	// Error codes
	CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
	CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
	CASE(VK_ERROR_INITIALIZATION_FAILED);
	CASE(VK_ERROR_DEVICE_LOST);
	CASE(VK_ERROR_MEMORY_MAP_FAILED);
	CASE(VK_ERROR_LAYER_NOT_PRESENT);
	CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
	CASE(VK_ERROR_FEATURE_NOT_PRESENT);
	CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
	CASE(VK_ERROR_TOO_MANY_OBJECTS);
	CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
	CASE(VK_ERROR_FRAGMENTED_POOL);
	CASE(VK_ERROR_UNKNOWN);
	CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
	CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
	CASE(VK_ERROR_FRAGMENTATION);
	CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
	CASE(VK_ERROR_SURFACE_LOST_KHR);
	CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
	CASE(VK_ERROR_OUT_OF_DATE_KHR);
	CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
	CASE(VK_ERROR_VALIDATION_FAILED_EXT);
	CASE(VK_ERROR_INVALID_SHADER_NV);
	CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
	CASE(VK_ERROR_NOT_PERMITTED_KHR);
	CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
	CASE(VK_ERROR_COMPRESSION_EXHAUSTED_EXT);
	// Just to satisfy -Wswitch
	CASE(VK_RESULT_MAX_ENUM);
	// No `default` to make `-Werror -Wswitch` protection work
	}
#undef CASE
}

std::string_view VulkanUtils::getVkFormatString(VkFormat format) noexcept
{
	using namespace std::string_view_literals;

	// The list is copy-pasted from Vulkan 1.2.170 headers.
	// Multiplane, some block, 4-bit, scaled and other crazy formats were removed.
#define FORMAT_ENTRY(fmt) case fmt: return #fmt##sv
	switch (format) {
	FORMAT_ENTRY(VK_FORMAT_UNDEFINED);
	FORMAT_ENTRY(VK_FORMAT_R8_UNORM);
	FORMAT_ENTRY(VK_FORMAT_R8_SNORM);
	FORMAT_ENTRY(VK_FORMAT_R8_UINT);
	FORMAT_ENTRY(VK_FORMAT_R8_SINT);
	FORMAT_ENTRY(VK_FORMAT_R8_SRGB);
	FORMAT_ENTRY(VK_FORMAT_R8G8_UNORM);
	FORMAT_ENTRY(VK_FORMAT_R8G8_SNORM);
	FORMAT_ENTRY(VK_FORMAT_R8G8_UINT);
	FORMAT_ENTRY(VK_FORMAT_R8G8_SINT);
	FORMAT_ENTRY(VK_FORMAT_R8G8_SRGB);
	FORMAT_ENTRY(VK_FORMAT_R8G8B8_UNORM);
	FORMAT_ENTRY(VK_FORMAT_R8G8B8_SNORM);
	FORMAT_ENTRY(VK_FORMAT_R8G8B8_UINT);
	FORMAT_ENTRY(VK_FORMAT_R8G8B8_SINT);
	FORMAT_ENTRY(VK_FORMAT_R8G8B8_SRGB);
	FORMAT_ENTRY(VK_FORMAT_B8G8R8_UNORM);
	FORMAT_ENTRY(VK_FORMAT_B8G8R8_SNORM);
	FORMAT_ENTRY(VK_FORMAT_B8G8R8_UINT);
	FORMAT_ENTRY(VK_FORMAT_B8G8R8_SINT);
	FORMAT_ENTRY(VK_FORMAT_B8G8R8_SRGB);
	FORMAT_ENTRY(VK_FORMAT_R8G8B8A8_UNORM);
	FORMAT_ENTRY(VK_FORMAT_R8G8B8A8_SNORM);
	FORMAT_ENTRY(VK_FORMAT_R8G8B8A8_UINT);
	FORMAT_ENTRY(VK_FORMAT_R8G8B8A8_SINT);
	FORMAT_ENTRY(VK_FORMAT_R8G8B8A8_SRGB);
	FORMAT_ENTRY(VK_FORMAT_B8G8R8A8_UNORM);
	FORMAT_ENTRY(VK_FORMAT_B8G8R8A8_SNORM);
	FORMAT_ENTRY(VK_FORMAT_B8G8R8A8_UINT);
	FORMAT_ENTRY(VK_FORMAT_B8G8R8A8_SINT);
	FORMAT_ENTRY(VK_FORMAT_B8G8R8A8_SRGB);
	FORMAT_ENTRY(VK_FORMAT_A8B8G8R8_UNORM_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A8B8G8R8_SNORM_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A8B8G8R8_UINT_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A8B8G8R8_SINT_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A8B8G8R8_SRGB_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A2R10G10B10_UNORM_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A2R10G10B10_SNORM_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A2R10G10B10_UINT_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A2R10G10B10_SINT_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A2B10G10R10_UNORM_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A2B10G10R10_SNORM_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A2B10G10R10_UINT_PACK32);
	FORMAT_ENTRY(VK_FORMAT_A2B10G10R10_SINT_PACK32);
	FORMAT_ENTRY(VK_FORMAT_R16_UNORM);
	FORMAT_ENTRY(VK_FORMAT_R16_SNORM);
	FORMAT_ENTRY(VK_FORMAT_R16_UINT);
	FORMAT_ENTRY(VK_FORMAT_R16_SINT);
	FORMAT_ENTRY(VK_FORMAT_R16_SFLOAT);
	FORMAT_ENTRY(VK_FORMAT_R16G16_UNORM);
	FORMAT_ENTRY(VK_FORMAT_R16G16_SNORM);
	FORMAT_ENTRY(VK_FORMAT_R16G16_UINT);
	FORMAT_ENTRY(VK_FORMAT_R16G16_SINT);
	FORMAT_ENTRY(VK_FORMAT_R16G16_SFLOAT);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16_UNORM);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16_SNORM);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16_USCALED);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16_SSCALED);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16_UINT);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16_SINT);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16_SFLOAT);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16A16_UNORM);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16A16_SNORM);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16A16_USCALED);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16A16_SSCALED);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16A16_UINT);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16A16_SINT);
	FORMAT_ENTRY(VK_FORMAT_R16G16B16A16_SFLOAT);
	FORMAT_ENTRY(VK_FORMAT_R32_UINT);
	FORMAT_ENTRY(VK_FORMAT_R32_SINT);
	FORMAT_ENTRY(VK_FORMAT_R32_SFLOAT);
	FORMAT_ENTRY(VK_FORMAT_R32G32_UINT);
	FORMAT_ENTRY(VK_FORMAT_R32G32_SINT);
	FORMAT_ENTRY(VK_FORMAT_R32G32_SFLOAT);
	FORMAT_ENTRY(VK_FORMAT_R32G32B32_UINT);
	FORMAT_ENTRY(VK_FORMAT_R32G32B32_SINT);
	FORMAT_ENTRY(VK_FORMAT_R32G32B32_SFLOAT);
	FORMAT_ENTRY(VK_FORMAT_R32G32B32A32_UINT);
	FORMAT_ENTRY(VK_FORMAT_R32G32B32A32_SINT);
	FORMAT_ENTRY(VK_FORMAT_R32G32B32A32_SFLOAT);
	FORMAT_ENTRY(VK_FORMAT_D16_UNORM);
	FORMAT_ENTRY(VK_FORMAT_X8_D24_UNORM_PACK32);
	FORMAT_ENTRY(VK_FORMAT_D32_SFLOAT);
	FORMAT_ENTRY(VK_FORMAT_S8_UINT);
	FORMAT_ENTRY(VK_FORMAT_D16_UNORM_S8_UINT);
	FORMAT_ENTRY(VK_FORMAT_D24_UNORM_S8_UINT);
	FORMAT_ENTRY(VK_FORMAT_D32_SFLOAT_S8_UINT);
	FORMAT_ENTRY(VK_FORMAT_BC1_RGB_UNORM_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC1_RGB_SRGB_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC2_UNORM_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC2_SRGB_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC3_UNORM_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC3_SRGB_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC4_UNORM_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC4_SNORM_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC5_UNORM_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC5_SNORM_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC6H_UFLOAT_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC6H_SFLOAT_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC7_UNORM_BLOCK);
	FORMAT_ENTRY(VK_FORMAT_BC7_SRGB_BLOCK);
	default: return "VK_FORMAT_[UNKNOWN]"sv;
	}
#undef FORMAT_ENTRY
}

}
