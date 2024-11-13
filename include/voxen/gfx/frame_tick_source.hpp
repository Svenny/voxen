#pragma once

#include <voxen/gfx/frame_tick_id.hpp>
#include <voxen/gfx/gfx_fwd.hpp>

#include <utility>
#include <vector>

namespace voxen::gfx
{

// A small helper tracking the current frame tick ID and GPU completion of past ticks
class FrameTickSource {
public:
	FrameTickSource();
	FrameTickSource(FrameTickSource &&) = delete;
	FrameTickSource(const FrameTickSource &) = delete;
	FrameTickSource &operator=(FrameTickSource &&) = delete;
	FrameTickSource &operator=(const FrameTickSource &) = delete;
	~FrameTickSource();

	// Does the following:
	// 1. Record all device command submission timelines associated with the current tick
	// 2. Advance the current tick ID by one
	// 3. Check previous timeline completions and update the last completed tick accordingly
	// 4. Return the pair (last completed tick; new tick)
	std::pair<FrameTickId, FrameTickId> startNextTick(GfxSystem &gfx);

	// Waits for `tick_id` to complete GPU execution.
	// Waiting for a tick not yet fully submitted (current or future value)
	// is a fatal bug and will cause program termination.
	void waitTickCompletion(GfxSystem &gfx, FrameTickId tick_id);

	FrameTickId currentTickId() const noexcept { return m_current_tick_id; }
	FrameTickId completedTickId() const noexcept { return m_completed_tick_id; }

private:
	struct TimelinePack;

	FrameTickId m_current_tick_id { 1 };
	FrameTickId m_completed_tick_id { 0 };

	std::vector<TimelinePack> m_pending_timeline_packs;
};

} // namespace voxen::gfx
