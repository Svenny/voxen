#pragma once

#include <cstdint>

namespace voxen
{

using voxel_t = uint8_t;

// Must match GLM order (X=0, Y=1, Z=2)
enum class Axis {
	X = 0,
	Y = 1,
	Z = 2
};

}
