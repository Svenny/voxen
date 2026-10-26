#pragma once

#include <tk/uid.hpp>

#include <fmt/base.h>

// fmt formatters for `tk` types, opt-in to keep fmt out of the core headers.

namespace fmt
{

template<>
struct formatter<tk::UID> : formatter<string_view> {
	format_context::iterator format(const tk::UID &uid, format_context &ctx) const;
};

} // namespace fmt
