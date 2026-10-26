#pragma once

#include <voxen/land/land_state.hpp>
#include <voxen/util/tick_id.hpp>
#include <voxen/world/player.hpp>

namespace voxen::world
{

class State {
public:
	State() = default;
	State(State &&other) = default;
	State(const State &other) = default;
	State &operator=(State &&) = delete;
	State &operator=(const State &) = delete;
	~State() = default;

	Player &player() noexcept { return m_player; }
	const Player &player() const noexcept { return m_player; }

	const land::LandState &landState() const noexcept { return m_land_state; }
	void setLandState(const land::LandState &state) { m_land_state = state; }

	WorldTickId tickId() const noexcept { return m_tick_id; }
	void setTickId(WorldTickId value) noexcept { m_tick_id = value; }

private:
	Player m_player;
	land::LandState m_land_state;
	WorldTickId m_tick_id { 0 };
};

} // namespace voxen::world
