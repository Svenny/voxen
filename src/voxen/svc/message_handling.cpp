#include <voxen/svc/message_handling.hpp>

#include "messaging_private.hpp"

namespace voxen::svc
{

UID MessageInfo::senderUid() const noexcept
{
	return m_hdr->from_uid;
}

} // namespace voxen::svc
