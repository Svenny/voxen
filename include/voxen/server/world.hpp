#pragma once

#include <voxen/common/terrain/loader.hpp>
#include <voxen/common/world_state.hpp>

#include <memory>

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

	std::shared_ptr<const WorldState> getLastState() const noexcept;

	double secondsPerTick() const noexcept { return 1.0 / 100.0; } // 100 UPS

	void update(DebugQueueRtW& queue, std::chrono::duration<int64_t, std::nano> tick_inverval);
private:
	TerrainLoader m_loader;

	std::shared_ptr<WorldState> m_next_state_ptr;
	std::shared_ptr<WorldState> m_last_state_ptr;
};

}
