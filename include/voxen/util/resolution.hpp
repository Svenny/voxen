#pragma once

#include <algorithm>
#include <cstdint>
#include <tuple>

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

template<size_t I>
inline auto &get(voxen::Resolution &res) noexcept
{
	if constexpr (I == 0) {
		return res.width;
	} else {
		return res.height;
	}
}

template<size_t I>
inline auto get(const voxen::Resolution &res) noexcept
{
	if constexpr (I == 0) {
		return res.width;
	} else {
		return res.height;
	}
}

} // namespace voxen

template<>
struct std::tuple_size<voxen::Resolution> : public integral_constant<size_t, 2> {};

template<size_t I>
struct std::tuple_element<I, voxen::Resolution> {
	using type = decltype(voxen::Resolution::width);
};
