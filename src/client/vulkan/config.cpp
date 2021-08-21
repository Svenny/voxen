#include <voxen/client/vulkan/config.hpp>

#include <bit>

namespace voxen::client::vulkan
{

// This file only contains some sanity checks of `Config` fields

static_assert(Config::NUM_GPU_PENDING_FRAMES > 0, "Number of GPU pending frames must be positive");
static_assert(Config::NUM_CPU_PENDING_FRAMES >= Config::NUM_GPU_PENDING_FRAMES,
              "Number of CPU pending frames must not be less than GPU pending frames");

static_assert(Config::ARENA_SIZE_INITIAL_GUESS > 0, "Arena size initial guess must be positive");
static_assert(Config::ARENA_SIZE_INITIAL_GUESS % Config::ARENA_SIZE_ALIGNMENT == 0,
              "Arena size initial guess must be properly aligned");

static_assert(Config::ARENA_GROW_FACTOR_DENOMINATOR > 0, "Arena grow factor denominator must be positive");
static_assert(Config::ARENA_GROW_FACTOR_NUMERATOR > Config::ARENA_GROW_FACTOR_DENOMINATOR,
              "Arena grow factor must be larger than one (numerator > denomenator)");

static_assert(std::has_single_bit(Config::ALLOCATION_GRANULARITY), "Allocation granularity must be a power of two");
static_assert(std::has_single_bit(Config::ARENA_SIZE_ALIGNMENT), "Arena size alignment must be a power of two");

}
