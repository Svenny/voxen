#pragma once

#include <voxen/gfx/vk/vk_include.hpp>

#include <string_view>

// Some stateless utility functions
namespace voxen::gfx::vk::VulkanUtils
{

std::string_view getVkResultString(VkResult result) noexcept;
std::string_view getVkFormatString(VkFormat format) noexcept;

// Returns true for depth-stencil formats with non-zero stencil part
bool hasStencilComponent(VkFormat format) noexcept;

// Returns minimal integer multiple of `alignment` not less than `size`. `alignment` must be a power of two.
uint32_t alignUp(uint32_t size, uint32_t alignment) noexcept;
uint64_t alignUp(uint64_t size, uint64_t alignment) noexcept;
// Multiplies `size` by `numerator` and then divides by `denomenator` with rounding up.
uint64_t calcFraction(uint64_t size, uint64_t numerator, uint64_t denomenator) noexcept;

} // namespace voxen::gfx::vk::VulkanUtils
