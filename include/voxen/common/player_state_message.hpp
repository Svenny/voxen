#pragma once

#include <voxen/svc/message_types.hpp>

#include <m/quaternion.hpp>

namespace voxen
{

// Sent from client to world to update server-side player state
struct PlayerStateMessage {
	constexpr static UID MESSAGE_UID = UID("55503c30-3aa166ee-62fe1b53-718fde15");
	constexpr static svc::MessageClass MESSAGE_CLASS = svc::MessageClass::Unicast;

	m::double3 player_position;
	m::dquat player_orientation;

	bool lock_chunk_loading_position = false;
};

} // namespace voxen
