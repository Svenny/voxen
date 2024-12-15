#pragma once

#include <voxen/land/chunk_key.hpp>
#include <voxen/land/chunk_ticket.hpp>
#include <voxen/svc/message_types.hpp>
#include <voxen/land/land_chunk.hpp>

namespace voxen::land
{

struct ChunkTicketRequestMessage {
	constexpr static UID MESSAGE_UID = UID("6caf08d9-5d4f47a4-816d9cc0-3f2c9d79");
	constexpr static svc::MessageClass MESSAGE_CLASS = svc::MessageClass::Request;

	// Requested coverage area for this ticket
	ChunkTicketArea area;
	// Ticket handle, will be filled by the service.
	// Move it away and store before destroying this message payload.
	ChunkTicket ticket;
};

struct BlockEditMessage {
	constexpr static UID MESSAGE_UID = UID("340d78cd-5a543514-8d4a8a15-de39ab3c");
	constexpr static svc::MessageClass MESSAGE_CLASS = svc::MessageClass::Unicast;

	glm::ivec3 position;
	Chunk::BlockId new_id;
};

} // namespace voxen::land
