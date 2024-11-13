#include <voxen/gfx/frame_tick_source.hpp>

#include <voxen/debug/bug_found.hpp>
#include <voxen/gfx/gfx_system.hpp>
#include <voxen/gfx/vk/vk_device.hpp>

#include <extras/enum_utils.hpp>

#include <cassert>

namespace voxen::gfx
{

struct FrameTickSource::TimelinePack {
	FrameTickId tick_id;
	uint64_t timelines[vk::Device::QueueCount];

	bool allComplete(const TimelinePack& completed_pack) const noexcept
	{
		for (size_t i = 0; i < std::size(timelines); i++) {
			if (timelines[i] > completed_pack.timelines[i]) {
				return false;
			}
		}

		return true;
	}
};

FrameTickSource::FrameTickSource() = default;
FrameTickSource::~FrameTickSource() = default;

std::pair<FrameTickId, FrameTickId> FrameTickSource::startNextTick(GfxSystem& gfx)
{
	using Queue = vk::Device::Queue;

	// Record last submitted timelines here
	TimelinePack current_tick_pack;
	current_tick_pack.tick_id = m_current_tick_id;

	// And completed timelines here
	TimelinePack completed_pack;

	vk::Device* dev = gfx.device();
	for (uint32_t q = 0; q < extras::to_underlying(Queue::QueueCount); q++) {
		current_tick_pack.timelines[q] = dev->getLastSubmittedTimeline(Queue(q));
		completed_pack.timelines[q] = dev->getCompletedTimeline(Queue(q));
	}

	// Pop completed frame tick IDs, they were inserted new-to-old
	while (!m_pending_timeline_packs.empty() && m_pending_timeline_packs.back().allComplete(completed_pack)) {
		m_completed_tick_id = m_pending_timeline_packs.back().tick_id;
		m_pending_timeline_packs.pop_back();
	}

	if (!current_tick_pack.allComplete(completed_pack)) [[likely]] {
		// Insert the current pack as pending to be popped somewhere later.
		// Should be at most a few items, having more means we're seriously ahead of GPU.
		m_pending_timeline_packs.insert(m_pending_timeline_packs.begin(), current_tick_pack);
	} else {
		// A rare (at least not expected) event - this frame is already GPU-completed.
		// Either the GPU is *very* fast or we did not submit anything during this frame.
		m_completed_tick_id = m_current_tick_id;
	}

	m_current_tick_id++;
	return { m_completed_tick_id, m_current_tick_id };
}

void FrameTickSource::waitTickCompletion(GfxSystem& gfx, FrameTickId tick_id)
{
	if (tick_id <= m_completed_tick_id) {
		// Already completed
		return;
	}

	if (tick_id >= m_current_tick_id) [[unlikely]] {
		// This is a fatal bug
		debug::bugFound("gfx - waiting for frame tick that is not yet fully submitted");
	}

	// Find this tick in the pending array
	auto iter = m_pending_timeline_packs.rbegin();
	while (iter != m_pending_timeline_packs.rend()) {
		if (iter->tick_id == tick_id) {
			break;
		}

		++iter;
	}

	// This tick is not considered completed, it must be in the array
	assert(iter != m_pending_timeline_packs.rend());

	gfx.device()->waitForTimelines(iter->timelines);
	// Pop this tick and all previous ones (array order is new-to-old)
	m_pending_timeline_packs.erase(iter.base(), m_pending_timeline_packs.end());
	m_completed_tick_id = tick_id;
}

} // namespace voxen::gfx
