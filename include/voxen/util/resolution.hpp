#pragma once

#include <algorithm>
#include <cstdint>

namespace voxen
{

struct Resolution {
	int32_t width = -1;
	int32_t height = -1;

	bool valid() const noexcept { return width > 0 && height > 0; }

	Resolution mip(int32_t level) const noexcept
	{
		return { std::max(1, width >> level), std::max(1, height >> level) };
	}
};

} // namespace voxen
