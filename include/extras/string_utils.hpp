#pragma once

#include <string_view>
#include <functional>

void string_split_apply(std::string_view string, std::string_view delimiter, std::function<void(std::string_view)> functor) {
	size_t prev_pos = 0;
	size_t pos = 0;
	while ((pos = string.find(delimiter, prev_pos)) != std::string::npos) {
		std::string_view subvalue = string.substr(prev_pos, pos - prev_pos);
		functor(subvalue);
		prev_pos = pos+delimiter.size();
	}
	//Handle rest of the string
	functor(string.substr(prev_pos, string.size()));
}
