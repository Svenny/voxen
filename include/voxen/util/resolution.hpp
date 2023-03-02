#pragma once

#include <cstdint>

namespace voxen
{

struct Resolution {
	int32_t width = -1;
	int32_t height = -1;

	bool valid() const noexcept { return width > 0 && height > 0; }
};

}
