#pragma once

#include <voxen/visibility.hpp>

#include <string_view>

namespace voxen::debug
{

// Set debugger name for the current thread. String must be pure ASCII.
// Might also be visible in stacktraces if we are lucky enough.
//
// Length limit is very tight (currently 15 characters), string will be truncated to it.
VOXEN_API void setThreadName(std::string_view name);

[[gnu::format(__printf__, 1, 2)]] VOXEN_API void setThreadName(const char *fmt, ...);

} // namespace voxen::debug
