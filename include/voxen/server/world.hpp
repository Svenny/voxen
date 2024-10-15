#pragma once

#include <voxen/common/terrain/controller.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/visibility.hpp>

#include <memory>

namespace voxen::server
{

class VOXEN_API World {
public:
	World();
	World(World &&) = delete;
	World(const World &) = delete;
	World &operator=(World &&) = delete;
	World &operator=(const World &) = delete;
	~World() noexcept;

	// Acquire a reference to the last complete state.
	// This function is thread-safe.
	std::shared_ptr<const WorldState> getLastState() const;

	double secondsPerTick() const noexcept { return 1.0 / 100.0; } // 100 UPS

	void update(DebugQueueRtW &queue, std::chrono::duration<int64_t, std::nano> tick_inverval);

private:
	terrain::Controller m_terrain_controller;

	// `getLastState()` and `update()` may be called from different
	// threads simultaneously. Therefore this pointer is atomic.
	std::atomic<std::shared_ptr<WorldState>> m_last_state_ptr;

	glm::dvec3 m_chunk_loading_position;
};

} // namespace voxen::server
