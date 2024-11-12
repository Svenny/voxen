#pragma once

#include <voxen/land/chunk_key.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/visibility.hpp>

#include <cstdint>
#include <variant>

namespace voxen::land
{

namespace detail
{

class LandServiceImpl;

}

// Defines axis-aligned bounding box area - every chunk at LOD `begin.scale_log2`
// having XYZ coordinates within `begin` (inclusive) and `end` (exclusive).
// `end.scale_log2` is ignored. If any coordinate of `begin` is greater than or equal
// to that of `end` then the area is empty and an invalid ticket will be returned.
struct ChunkTicketBoxArea {
	ChunkKey begin;
	ChunkKey end;

	bool operator==(const ChunkTicketBoxArea &other) const = default;
};

// Defines octahedral area - every chunk at LOD `pivot.scale_log2`
// having XYZ coordinates within `scaled_radius << pivot.scale_log2` of `pivot`.
// If `scaled_radius == 0` then the area is empty and an invalid ticket will be returned.
struct ChunkTicketOctahedronArea {
	ChunkKey pivot;
	uint8_t scaled_radius;

	bool operator==(const ChunkTicketOctahedronArea &other) const = default;
};

using ChunkTicketArea = std::variant<ChunkTicketBoxArea, ChunkTicketOctahedronArea>;

class VOXEN_API ChunkTicket {
public:
	constexpr static uint64_t INVALID_TICKET_ID = ~uint64_t(0);

	ChunkTicket() = default;
	ChunkTicket(ChunkTicket &&) noexcept;
	ChunkTicket(const ChunkTicket &) = delete;
	ChunkTicket &operator=(ChunkTicket &&) noexcept;
	ChunkTicket &operator=(const ChunkTicket &) = delete;
	~ChunkTicket();

	bool valid() const noexcept { return m_ticket_id != INVALID_TICKET_ID; }

	void adjustAsync(ChunkTicketBoxArea new_box);
	void adjustAsync(ChunkTicketOctahedronArea new_octahedron);

private:
	uint64_t m_ticket_id = INVALID_TICKET_ID;
	svc::MessageSender *m_sender = nullptr;

	ChunkTicket(uint64_t id, svc::MessageSender *sender) noexcept;
	friend class detail::LandServiceImpl;
};

} // namespace voxen::land
