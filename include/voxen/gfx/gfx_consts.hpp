#pragma once

#include <cstdint>

// Publicly accessible constants of Gfx subsystem
namespace voxen::gfx::Consts
{

// Maximal number of frames in flight including both CPU and GPU.
// This defines the size of per-frame resource ring buffers.
//
// In `FrameTickId` terms, this defines the maximal possible
// difference between the current and the last completed tick IDs
// before a blocking wait for frame completion would occur.
constexpr uint32_t MAX_PENDING_FRAMES = 3;

} // namespace voxen::gfx::Consts
