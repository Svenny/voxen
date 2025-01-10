#pragma once

#include <voxen/visibility.hpp>

#include <ctime>

namespace voxen::os
{

namespace Time
{

// High-resolution sleep for a given (relative) amount of time.
// Can allow for microsecond sleep precision if supported by OS/hardware.
// Returns `true` if the sleep ended and `false` if it was interrupted.
VOXEN_API bool nanosleepFor(struct timespec timeout) noexcept;

} // namespace Time

} // namespace voxen::os
