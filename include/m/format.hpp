#pragma once

#include <m/matrix.hpp>
#include <m/quaternion.hpp>

#include <fmt/format.h>

// fmt formatters for `m` types, opt-in to keep fmt out of the core headers.
// The format spec applies to every component: `{:.1f}` on `float3(1, 2, 3)`
// gives `(1.0, 2.0, 3.0)`. Matrices are printed as a list of rows and
// quaternions as `(x, y, z, w)`.

namespace fmt
{

template<typename T, size_t N>
struct formatter<m::vector<T, N>> : formatter<T> {
	template<typename FormatContext>
	auto format(const m::vector<T, N> &v, FormatContext &ctx) const
	{
		auto out = ctx.out();
		*out++ = '(';
		for (size_t i = 0; i < N; i++) {
			if (i > 0) {
				*out++ = ',';
				*out++ = ' ';
			}
			ctx.advance_to(out);
			out = formatter<T>::format(v[i], ctx);
		}
		*out++ = ')';
		return out;
	}
};

template<typename T, size_t R, size_t C>
struct formatter<m::matrix<T, R, C>> : formatter<m::vector<T, C>> {
	template<typename FormatContext>
	auto format(const m::matrix<T, R, C> &a, FormatContext &ctx) const
	{
		auto out = ctx.out();
		*out++ = '[';
		for (size_t i = 0; i < R; i++) {
			if (i > 0) {
				*out++ = ',';
				*out++ = ' ';
			}
			ctx.advance_to(out);
			out = formatter<m::vector<T, C>>::format(a[i], ctx);
		}
		*out++ = ']';
		return out;
	}
};

template<typename T>
struct formatter<m::quaternion<T>> : formatter<m::vector<T, 4>> {
	template<typename FormatContext>
	auto format(const m::quaternion<T> &q, FormatContext &ctx) const
	{
		return formatter<m::vector<T, 4>>::format(q.toVector(), ctx);
	}
};

} // namespace fmt
