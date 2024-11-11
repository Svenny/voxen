#pragma once

#include <voxen/land/chunk_key.hpp>
#include <voxen/land/chunk_ticket.hpp>
#include <voxen/land/land_state.hpp>
#include <voxen/svc/message_types.hpp>

namespace voxen::land::detail
{

// Sent by `ChunkTicket` when its owner wants to adjust it asynchronously
struct ChunkTicketAdjustMessage {
	constexpr static UID MESSAGE_UID = UID("2398d8ff-c3af7864-544755df-adfd2173");
	constexpr static svc::MessageClass MESSAGE_CLASS = svc::MessageClass::Unicast;

	uint64_t ticket_id;
	ChunkTicketArea new_area;
};

// Sent automatically by `ChunkTicket` when destroying
struct ChunkTicketRemoveMessage {
	constexpr static UID MESSAGE_UID = UID("a2e6579d-c07cbb78-58031ca9-37bae862");
	constexpr static svc::MessageClass MESSAGE_CLASS = svc::MessageClass::Unicast;

	uint64_t ticket_id;
};

// Sent from slave threads upon chunk load job completion
struct ChunkLoadCompletionMessage {
	constexpr static UID MESSAGE_UID = UID("3fe5c4f7-9db2a3da-cdf92c68-91e567fa");
	constexpr static svc::MessageClass MESSAGE_CLASS = svc::MessageClass::Unicast;

	ChunkKey key;
	LandState::ChunkTable::ValuePtr value_ptr;
};

// Sent from slave threads upon pseudo-chunk data gen job completion
struct PseudoChunkDataGenCompletionMessage {
	constexpr static UID MESSAGE_UID = UID("921efbbd-863d267a-f4063130-218f6b30");
	constexpr static svc::MessageClass MESSAGE_CLASS = svc::MessageClass::Unicast;

	ChunkKey key;
	LandState::PseudoChunkDataTable::ValuePtr value_ptr;
};

// Sent from slave thread upon pseudo-chunk surface gen job completion
struct PseudoChunkSurfaceGenCompletionMessage {
	constexpr static UID MESSAGE_UID = UID("d4c5572d-9655ada3-83ea228d-46c278c4");
	constexpr static svc::MessageClass MESSAGE_CLASS = svc::MessageClass::Unicast;

	ChunkKey key;
	LandState::PseudoChunkSurfaceTable::ValuePtr value_ptr;
};

} // namespace voxen::land::detail
