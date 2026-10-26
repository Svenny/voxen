#pragma once

#include <voxen/exception.hpp>
#include <voxen/gfx/vk/vk_include.hpp>
#include <voxen/visibility.hpp>

#include <source_location>
#include <system_error>
#include <type_traits>

namespace std
{

// Mark `VkResult` as eligible for `std::error_code`.
// It has its own error category that maps to `std::error_condition`.
template<>
struct is_error_code_enum<VkResult> : true_type {};

// Factory for `std::error_code { VkResult }`
error_code make_error_code(VkResult result) noexcept;

} // namespace std

namespace voxen::gfx::vk
{

// Convert `VkResult` error codes returned from API calls into exceptions.
// Its `what()` message shows failed Vulkan function and error enum name.
class VOXEN_API VulkanException final : public Exception {
public:
	// `api` must be the name of Vulkan function that returned `result`
	explicit VulkanException(VkResult result, const char *api,
		std::source_location loc = std::source_location::current());
	VulkanException(VulkanException &&) = default;
	VulkanException(const VulkanException &) = default;
	VulkanException &operator=(VulkanException &&) = default;
	VulkanException &operator=(const VulkanException &) = default;
	~VulkanException() noexcept override;

	VkResult result() const noexcept { return m_result; }

private:
	VkResult m_result = VK_SUCCESS;
};

} // namespace voxen::gfx::vk
