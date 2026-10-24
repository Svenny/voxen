#pragma once

#include <m/m.hpp>

#include <cstddef>
#include <cstdint>

// GPU data structures declared once in `src/shaders/shared/*.h` and compiled both by
// slangc and by the C++ compiler. Their Slang (HLSL) type names map to `m` types here.
// `m` has the same memory layout as Slang with `-matrix-layout-row-major -fvk-use-scalar-layout`:
// tightly packed vectors, row-major matrices, column vectors (`mul(matrix, vector)`).

namespace voxen::gfx::shader
{

using uint = uint32_t;
using float2 = m::float2;
using float3 = m::float3;
using float4 = m::float4;
using float4x4 = m::float4x4;

#include "../../../shaders/shared/main_scene.h"

static_assert(sizeof(MainSceneUbo) == 80);
static_assert(offsetof(MainSceneUbo, world_position) == 64);

} // namespace voxen::gfx::shader
