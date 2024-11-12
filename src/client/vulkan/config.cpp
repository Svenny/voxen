#include <voxen/client/vulkan/config.hpp>

#include <voxen/common/terrain/config.hpp>

#include <bit>

namespace voxen::client::vulkan
{

// This file only contains some sanity checks of `Config` fields

static_assert(Config::NUM_GPU_PENDING_FRAMES > 0, "Number of GPU pending frames must be positive");
static_assert(Config::NUM_CPU_PENDING_FRAMES >= Config::NUM_GPU_PENDING_FRAMES,
	"Number of CPU pending frames must not be less than GPU pending frames");

static_assert(Config::MAX_RENDERED_CHUNKS >= 2048, "It's too dangerous to set low rendered chunks counts");
// Arbitrary value. This is to not bother with the last SIMD line possibly going out of bounds.
static_assert(Config::MAX_RENDERED_CHUNKS % 128 == 0, "Rendered chunks count should be a multiple of 128");

// This number is entirely not expected to ever happen in a real gameplay.
constexpr static size_t MAX_EXPECTED_VERTICES = terrain::Config::CHUNK_SIZE * terrain::Config::CHUNK_SIZE
	* terrain::Config::CHUNK_SIZE;
// NOTE: it's an approximation based on the fact that classical Dual Contouring builds 6 triangles per vertex.
constexpr static size_t MAX_EXPECTED_INDICES = 6 * MAX_EXPECTED_VERTICES;

static_assert(Config::MAX_TERRAIN_ARENA_VERTICES >= MAX_EXPECTED_VERTICES, "Arena size is dangerously low");
static_assert(Config::MAX_TERRAIN_ARENA_INDICES >= MAX_EXPECTED_INDICES, "Arena size is dangerously low");
static_assert(Config::TERRAIN_PER_FRAME_GC_STEPS > 0, "GC steps number must be positive");
static_assert(Config::TERRAIN_GC_AGE_THRESHOLD > Config::NUM_CPU_PENDING_FRAMES,
	"GC age threshold must be bigger than number of CPU frames");

} // namespace voxen::client::vulkan
