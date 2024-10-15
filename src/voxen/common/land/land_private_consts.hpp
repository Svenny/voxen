#pragma once

#include <voxen/common/land/land_public_consts.hpp>

namespace voxen::land::Consts
{

constexpr size_t MAX_CHECKED_COMPLETIONS_PER_TICK = 1280;
constexpr size_t MAX_PROCESSED_COMPLETIONS_PER_TICK = 640;

constexpr size_t MAX_LOADING_ITERATIONS_PER_TICK = 96;
constexpr size_t MAX_UNLOADING_ITERATIONS_PER_TICK = 256;

constexpr int32_t CHUNK_LOAD_DISTANCE_CHUNKS = 12;
// Adding some hysteresis to prevent load-unload bouncing
constexpr int32_t CHUNK_UNLOAD_DISTANCE_CHUNKS = CHUNK_LOAD_DISTANCE_CHUNKS + 3;

constexpr double CHUNKS_LOAD_DISTANCE = 0.75 * REGION_SIZE_METRES;
constexpr double IMPOSTORS_LOAD_DISTANCE = 7.0 * REGION_SIZE_METRES;

} // namespace voxen::land::Consts
