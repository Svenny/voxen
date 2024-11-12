#pragma once

#include <voxen/gfx/vk/vk_include.hpp>
#include <voxen/util/exception.hpp>

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

namespace voxen::gfx::vk
{

// Convert `VkResult` error codes returned from API calls into exceptions
class VOXEN_API VulkanException final : public Exception {
public:
	explicit VulkanException(VkResult result, std::string_view api,
		extras::source_location loc = extras::source_location::current());
	VulkanException(VulkanException &&) = default;
	VulkanException(const VulkanException &) = default;
	VulkanException &operator=(VulkanException &&) = default;
	VulkanException &operator=(const VulkanException &) = default;
	~VulkanException() noexcept override;

	VkResult result() const noexcept;
};

} // namespace voxen::gfx::vk
