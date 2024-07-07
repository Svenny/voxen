#pragma once

#include <vulkan/vulkan.h>

namespace voxen::gfx::vk::Consts
{

// Initial guess of the number of descriptor sets.
// Ideally it should be just enough to fit all sets used during one frame.
// Otherwise we will create more descriptor pools.
//
// As layouts (descriptor counts) can vary wildly, this number is not
// really related to the number of allocated sets. It's rather a
// "scale factor" for the pool (descriptor buffer) sizing, hence this name.
constexpr uint32_t DESCRIPTOR_POOL_SCALE_FACTOR = 128;
// Guessing the average descriptor counts in a single set.
// In practice they don't matter, only the total size (bytes) does.
// On modern hardware it's just a descriptor buffer under the hood.
//
// Basically these counts turn into X bytes per set, then we scale
// it by the expected number of sets, getting the total buffer size.
constexpr VkDescriptorPoolSize DESCRIPTOR_POOL_SIZING[] = {
	{ VK_DESCRIPTOR_TYPE_SAMPLER, 4 * DESCRIPTOR_POOL_SCALE_FACTOR },
	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * DESCRIPTOR_POOL_SCALE_FACTOR },
	{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4 * DESCRIPTOR_POOL_SCALE_FACTOR },
	{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * DESCRIPTOR_POOL_SCALE_FACTOR },
	{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1 * DESCRIPTOR_POOL_SCALE_FACTOR },
	{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1 * DESCRIPTOR_POOL_SCALE_FACTOR },
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 * DESCRIPTOR_POOL_SCALE_FACTOR },
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 * DESCRIPTOR_POOL_SCALE_FACTOR },
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2 * DESCRIPTOR_POOL_SCALE_FACTOR },
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 2 * DESCRIPTOR_POOL_SCALE_FACTOR },
	{ VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 256 * DESCRIPTOR_POOL_SCALE_FACTOR },
};

// Initial guess of constant upload buffer size.
// Ideally it should be just enough to fit all constant uploads made
// during one frame; if it doesn't, we will allocate more buffers.
constexpr VkDeviceSize CONST_UPLOAD_BUFFER_STARTING_SIZE = 32 * 1024;
// When allocating more constant upload buffers, apply this grow factor.
// Not really needed unless we badly underestimate the initial guess.
inline VkDeviceSize growConstUploadBufferSize(VkDeviceSize size) noexcept
{
	return size + size / 2; // Grow 1.5x
}
// During constant upload buffer fusing (combining multiple buffers' sizes
// into a new one), add some extra bytes to slightly speed up the convergence.
inline VkDeviceSize addConstUploadBufferFusing(VkDeviceSize size) noexcept
{
	return size + size / 16; // Add 6.25%
}

} // namespace voxen::gfx::vk::Consts
