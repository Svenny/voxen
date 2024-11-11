#pragma once

#include <voxen/land/chunk_key.hpp>
#include <voxen/land/chunk_ticket.hpp>
#include <voxen/svc/message_types.hpp>

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

} // namespace voxen::land
