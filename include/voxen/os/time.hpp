#pragma once

#include <voxen/visibility.hpp>

#include <cstdint>
#include <ctime>

namespace voxen::os::Time
{

// High-resolution sleep for a given (relative) amount of time.
// Can allow for microsecond sleep precision if supported by OS/hardware.
// Returns `true` if the sleep ended and `false` if it was interrupted.
VOXEN_API bool nanosleepFor(struct timespec timeout) noexcept;

// Calendar time in the local timezone, with the timezone information
// which `std::tm` doesn't have on Windows (fmt can't format `%z` and `%Z` there)
struct LocalTime {
	std::tm tm;
	// Offset from UTC including DST, positive east of Greenwich
	int32_t utc_offset_minutes;
	// Timezone name as the OS reports it, e.g. "CEST" or "W. Europe Daylight Time"
	// (localized on Windows). UTF-8, null-terminated, empty if unknown.
	char zone_name[128];
};

// Thread-safe wrapper for `localtime()`, converts time point to calendar time in local timezone.
// Returns zero-initialized structure if the conversion fails.
VOXEN_API LocalTime localTime(std::time_t time) noexcept;

} // namespace voxen::os::Time
