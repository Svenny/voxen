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

	// --- Terrain rendering parameters ---

	// This is a hard limit on the number of chunks which can be rendered during one frame.
	// Having this limit significantly simplifies some parts of data synchronization logic.
	// Exceeding it should be treated as a bug and fixed simply by raising the value.
	// Though it is not at all expected any sane scene setup can reach this number.
	constexpr static size_t MAX_RENDERED_CHUNKS = 2048;
	// The maximal number of vertices which can fit in a single terrain mesh arena.
	// Increasing it will allow using fewer arenas (and therefore render the whole
	// terrain in less drawcalls) at the cost of possibly increased VRAM waste.
	// It's theoretically possible that a chunk with extremely complex surface can exceed
	// arena size alone. This should be treated as a bug and fixed by raising the value.
	// Though it is not at all expected any sane surface can reach this number.
	constexpr static size_t MAX_TERRAIN_ARENA_VERTICES = 1024 * 1024;
	// The maximal number of UINT16 indices which can fit in a single terrain mesh arena.
	// Increasing it will allow using fewer arenas (and therefore render the whole
	// terrain in less drawcalls) at the cost of possibly increased VRAM waste.
	// It's theoretically possible that a chunk with extremely complex surface can exceed
	// arena size alone. This should be treated as a bug and fixed by raising the value.
	// Though it is not at all expected any sane surface can reach this number.
	constexpr static size_t MAX_TERRAIN_ARENA_INDICES = 6 * MAX_TERRAIN_ARENA_VERTICES;
	// Number of chunks checked for being outdated in a single frame (single sync session specifically).
	// Each visited chunk gets its "age" counter incremented. This counter is reset when
	// chunk is requested to synchronize with CPU data. When counter value reaches
	// certain threshold, a chunk is considered unused and gets removed from GPU storage.
	// This is just a minor performance tunable and does not affect functionality.
	constexpr static uint32_t TERRAIN_PER_FRAME_GC_STEPS = 4;
	// Threshold for counter value to consider chunks unused.
	// For some kind of safety it should be larger than number of CPU frames.
	// This is just a minor performance tunable and does not affect functionality.
	constexpr static uint32_t TERRAIN_GC_AGE_THRESHOLD = 16;
};

}
