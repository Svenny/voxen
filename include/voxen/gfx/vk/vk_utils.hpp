#pragma once

#include <voxen/gfx/vk/vk_include.hpp>
#include <voxen/gfx/gfx_fwd.hpp>

#include <array>
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

// All buffers can have concurrent sharing across all queue families,
// because every GPU driver known to me ignores that value for buffers.
// This function will fill `sharingMode`, `queueFamilyIndexCount` and `pQueueFamilyIndices`
// fields depending on the device info to be formally correct per Vulkan spec.
void fillBufferSharingInfo(Device &dev, VkBufferCreateInfo &info) noexcept;

// Returns a pseudo-random letter triplet in [A-Z] range to disambiguate
// this handle from others (~50% collision probability at 17576 handles).
// The fourth item is always '\0' to make it a null-terminated string.
std::array<char, 4> makeHandleDisambiguationString(void *handle) noexcept;

} // namespace voxen::gfx::vk::VulkanUtils
