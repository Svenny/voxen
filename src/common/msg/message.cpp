#include <voxen/common/msg/message.hpp>

namespace voxen::msg
{

// Message payloads are close to useless if they can't even hold a pointer
static_assert(sizeof(MessagePayloadStore) >= sizeof(void *), "Payload store is too small");
static_assert(sizeof(Message) == Config::MESSAGE_SIZE, "MESSAGE_SIZE is not respected");
static_assert(std::is_trivially_copyable_v<Message>, "Message type must be trivially copyable");

} // namespace voxen::msg
