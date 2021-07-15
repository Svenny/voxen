#pragma once

#include <voxen/common/terrain/controller.hpp>
#include <voxen/common/world_state.hpp>

#include <memory>
#include <mutex>

namespace voxen::server
{

class World {
public:
	World();
	World(World &&) = delete;
	World(const World &) = delete;
	World &operator = (World &&) = delete;
	World &operator = (const World &) = delete;
	~World() noexcept;

	std::shared_ptr<const WorldState> getLastState() const;

	double secondsPerTick() const noexcept { return 1.0 / 100.0; } // 100 UPS

	void update(DebugQueueRtW& queue, std::chrono::duration<int64_t, std::nano> tick_inverval);
private:
	terrain::Controller m_terrain_controller;

	// `getLastState()` and `update()` may be called from different
	// threads simultaneously. This mutex protects from data race
	// which may be caused by these methods executing non-atomic
	// operations (copying/changing owned object) on `m_last_state_ptr`.
	// TODO: replace with std::atomic<std::shared_ptr> when it's supported
	mutable std::mutex m_last_state_ptr_lock;
	std::shared_ptr<WorldState> m_next_state_ptr;
	std::shared_ptr<WorldState> m_last_state_ptr;
};

}
