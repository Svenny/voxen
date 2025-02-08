#pragma once

#include <extras/function_ref.hpp>

#include <string_view>

namespace extras
{

inline void string_split_apply(std::string_view string, std::string_view delimiter,
	function_ref<void(std::string_view)> functor)
{
	size_t prev_pos = 0;
	size_t pos = 0;
	while ((pos = string.find(delimiter, prev_pos)) != std::string::npos) {
		std::string_view subvalue = string.substr(prev_pos, pos - prev_pos);
		functor(subvalue);
		prev_pos = pos + delimiter.size();
	}
	//Handle rest of the string
	functor(string.substr(prev_pos, string.size()));
}

// Trivial cast helper to adapt `char` to `char8_t` when input contains only ASCII
inline std::u8string_view ascii_as_utf8(std::string_view view) noexcept
{
	return std::u8string_view(reinterpret_cast<const char8_t *>(view.data()), view.length());
}

} // namespace extras
