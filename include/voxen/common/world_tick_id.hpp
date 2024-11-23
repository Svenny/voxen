#pragma once

#include <voxen/util/tagged_tick_id.hpp>

namespace voxen
{

struct WorldTickTag {};

// World is simulated in steps called ticks.
//
// World state communicated to other subsystems is like a snapshot
// of the world at a time point associated with a certain tick ID.
using WorldTickId = TaggedTickId<WorldTickTag>;

} // namespace voxen
