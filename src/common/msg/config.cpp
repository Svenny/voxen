#include <voxen/common/msg/config.hpp>

namespace voxen::msg
{

// This file only contains some sanity checks of `Config` fields

static_assert(Config::MESSAGE_SIZE > sizeof(void *), "Message size is close to useless");
static_assert(Config::NUM_QUEUE_SHARDS > 0, "Queue must have at least one shard");
static_assert(Config::QUEUE_SEGMENT_SIZE > 0, "Queue segment must hold at least one message");

} // namespace voxen::msg
