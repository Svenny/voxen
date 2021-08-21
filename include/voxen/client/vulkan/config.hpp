#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

namespace voxen::client::vulkan
{

// Provides tunable constants for Vulkan rendering subsystem, all in one place
class Config final {
public:
	Config() = delete;

	// --- Main parameters ---

	// Maximal number of frames which can be in-flight simultaneously from CPU point of view. This controls the number
	// of CPU-filled data structures such as command buffers, descriptor sets, uniform/indirect buffers and so on.
	constexpr static uint32_t NUM_CPU_PENDING_FRAMES = 2;
	// Maximal number of frames which can be in-flight simultaneously from GPU point of view. This controls
	// the number of GPU-located data structures such as render targets and storage buffers/images.
	constexpr static uint32_t NUM_GPU_PENDING_FRAMES = 1;

	// --- Memory allocation parameters ---

	// If set to `true` then `HostAllocator::callbacks()` will return a valid instance of
	// Vulkan allocation callbacks structure. Otherwise it will return null pointer.
	// Tracking allocations can help detect Vulkan resource leaks (or driver bugs).
	constexpr static bool TRACK_HOST_ALLOCATIONS = false;

	// This is the starting size of arena for each memory type. It should be
	// pretty small to avoid large memory waste for rarely-used memory types.
	constexpr static VkDeviceSize ARENA_SIZE_INITIAL_GUESS = 32 * 1024 * 1024; // 32 MB
	// When all arenas for a given memory type are exhausted, a new one must be allocated.
	// This indicates more demand for this particular memory type, so a new arena should
	// probably be bigger than previous ones. Arena size is multiplied by this grow factor.
	constexpr static VkDeviceSize ARENA_GROW_FACTOR_NUMERATOR = 3;
	// This is the denominator of the fraction described above. Must be less than numerator.
	constexpr static VkDeviceSize ARENA_GROW_FACTOR_DENOMINATOR = 2;

	// The minimal size, in bytes, of a single allocation. This is needed to prevent
	// possible fragmentation of heaps will lots of tiny allocations. Allocator users
	// are nonetheless advised to request fewer allocations and reuse them when possible.
	// This is just a minor performance tunable and does not affect functionality.
	constexpr static VkDeviceSize ALLOCATION_GRANULARITY = 64;
	// An initial guess on the number of allocations made in a single memory arena.
	// This is just a minor performance tunable and does not affect functionality.
	constexpr static size_t EXPECTED_PER_ARENA_ALLOCATIONS = 128;
	// Each allocation arena size is aligned up to the multiple of this value.
	// This is just a minor performance tunable and does not affect functionality.
	constexpr static VkDeviceSize ARENA_SIZE_ALIGNMENT = 4096;
};

}