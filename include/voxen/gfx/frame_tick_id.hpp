#pragma once

#include <voxen/util/tagged_tick_id.hpp>

namespace voxen::gfx
{

struct FrameTickTag {};

// Gfx module measures its timeline in frames.
//
// One frame does not necessarily mean one present (e.g. there can be several
// presents if we ever decide to support auxiliary windows; or there can be none
// in headless rendering mode) but rather something like "rendering one or
// more views of a world state at a single time point". Though in most cases
// this does indeed mean one swapchain present.
//
// Frame boundaries serve as a common synchronization point for all CPU and GPU
// timelines involved. There is always one "current" CPU frame tick ID tracked by
// `FrameTickSource`, and some "previous" ticks can be executing on GPU.
// Also the last "completed" tick is tracked, and in most cases it will lag behind
// the "current" one by a few frames. Resources associated with completed frames
// can be freely released or recycled without the risk of data race with the GPU.
using FrameTickId = TaggedTickId<FrameTickTag>;

} // namespace voxen::gfx
