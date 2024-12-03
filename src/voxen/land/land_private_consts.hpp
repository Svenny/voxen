#pragma once

#include <voxen/common/uid.hpp>
#include <voxen/land/land_public_consts.hpp>

namespace voxen::land::Consts
{

// Used by `ChunkTicket` to inform land service of its adjustment/removal.
// Also used by slave threads to inform land service of job completions.
// Basically shared for every related object outside of `LandService` instance.
constexpr UID LAND_SERVICE_SENDER_UID = UID("e242afb4-eb63b2c0-f82103c1-85324c1c");

constexpr int32_t MAX_TICKET_BOX_AREA_SIZE = 24;
constexpr int32_t MAX_TICKET_OCTA_AREA_RADIUS = 16;

constexpr uint32_t MAX_DIRECT_GENERATE_LOD = NUM_LOD_SCALES - 3;

} // namespace voxen::land::Consts
