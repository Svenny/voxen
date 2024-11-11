#include <voxen/land/chunk_ticket.hpp>

#include <voxen/land/land_service.hpp>
#include <voxen/svc/message_sender.hpp>

#include "land_private_messages.hpp"

#include <cassert>

namespace voxen::land
{

ChunkTicket::ChunkTicket(ChunkTicket&& other) noexcept
	: m_ticket_id(std::exchange(other.m_ticket_id, INVALID_TICKET_ID)), m_sender(std::exchange(other.m_sender, nullptr))
{}

ChunkTicket& ChunkTicket::operator=(ChunkTicket&& other) noexcept
{
	std::swap(m_ticket_id, other.m_ticket_id);
	std::swap(m_sender, other.m_sender);
	return *this;
}

ChunkTicket::~ChunkTicket()
{
	if (m_sender && m_ticket_id != INVALID_TICKET_ID) {
		m_sender->send<detail::ChunkTicketRemoveMessage>(LandService::SERVICE_UID, m_ticket_id);
	}
}

void ChunkTicket::adjustAsync(ChunkTicketBoxArea new_box)
{
	assert(m_sender && m_ticket_id != INVALID_TICKET_ID);
	m_sender->send<detail::ChunkTicketAdjustMessage>(LandService::SERVICE_UID, m_ticket_id, new_box);
}

void ChunkTicket::adjustAsync(ChunkTicketOctahedronArea new_octahedron)
{
	assert(m_sender && m_ticket_id != INVALID_TICKET_ID);
	m_sender->send<detail::ChunkTicketAdjustMessage>(LandService::SERVICE_UID, m_ticket_id, new_octahedron);
}

ChunkTicket::ChunkTicket(uint64_t id, svc::MessageSender* sender) noexcept : m_ticket_id(id), m_sender(sender) {}

} // namespace voxen::land
