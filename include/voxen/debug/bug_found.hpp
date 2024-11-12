#pragma once

#include <voxen/visibility.hpp>

#include <extras/source_location.hpp>

#include <string_view>

namespace voxen::debug
{

// Call when a bug was expected and happened, e.g. on assert failure.
//
// Prints log message with stacktrace and explanatory message,
// requests user to create a bugreport and then calls `abort()`.
[[noreturn]] VOXEN_API void bugFound(std::string_view message = "",
	extras::source_location where = extras::source_location::current());

} // namespace voxen::debug
