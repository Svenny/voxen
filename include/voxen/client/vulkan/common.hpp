#pragma once

#include <voxen/util/exception.hpp>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <atomic>
#include <string>
#include <string_view>
#include <system_error>

namespace std
{

// Mark `VkResult` as eligible for `std::error_condition`
template<>
struct is_error_condition_enum<VkResult> : true_type {};

// Factory for `std::error_condition { VkResult }`
error_condition make_error_condition(VkResult result) noexcept;

} // namespace std

namespace voxen::client::vulkan
{

// Some stateless utility functions
class VulkanUtils final {
public:
	VulkanUtils() = delete;

	static std::string_view getVkResultString(VkResult result) noexcept;
	static std::string_view getVkFormatString(VkFormat format) noexcept;

	// Returns true for depth-stencil formats with non-zero stencil part
	static bool hasStencilComponent(VkFormat format) noexcept;

	// Returns minimal integer multiple of `alignment` not less than `size`. `alignment` must be a power of two.
	static uint32_t alignUp(uint32_t size, uint32_t alignment) noexcept;
	static uint64_t alignUp(uint64_t size, uint64_t alignment) noexcept;
	// Multiplies `size` by `numerator` and then divides by `denomenator` with rounding up.
	static uint64_t calcFraction(uint64_t size, uint64_t numerator, uint64_t denomenator) noexcept;
};

class VOXEN_API VulkanException final : public Exception {
public:
	explicit VulkanException(VkResult result, std::string_view api,
		extras::source_location loc = extras::source_location::current());
	~VulkanException() noexcept override;

	VkResult result() const noexcept;
};

// This is leftover from host memory allocation tracker.
// Removed as it was pretty useless, though maybe could be reinstated later.
// Remove these lines after purging remaining `callbacks()` call sites.
class HostAllocator {
public:
	static const VkAllocationCallbacks *callbacks() noexcept { return nullptr; }
};

} // namespace voxen::client::vulkan
