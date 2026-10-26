#include <voxen/os/time.hpp>

#include <voxen/debug/bug_found.hpp>
#include <voxen/log.hpp>

#ifdef _WIN32
	#include <Windows.h>
#endif

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <iterator>

namespace voxen::os
{

Time::LocalTime Time::localTime(std::time_t time) noexcept
{
	LocalTime result {};
#ifndef _WIN32
	if (!localtime_r(&time, &result.tm)) {
		return {};
	}

	result.utc_offset_minutes = static_cast<int32_t>(result.tm.tm_gmtoff / 60);
	if (result.tm.tm_zone) {
		snprintf(result.zone_name, std::size(result.zone_name), "%s", result.tm.tm_zone);
	}
#else
	if (localtime_s(&result.tm, &time) != 0) {
		return {};
	}

	// UCRT `std::tm` has no offset or zone fields. Reinterpreting the local
	// calendar time as UTC gives the offset, DST included.
	std::tm tm_copy = result.tm;
	const std::time_t local_as_utc = _mkgmtime(&tm_copy);
	if (local_as_utc != -1) {
		result.utc_offset_minutes = static_cast<int32_t>((local_as_utc - time) / 60);
	}

	// Not `_get_tzname()`, it returns the name (localized) in the ANSI code page, not UTF-8
	DYNAMIC_TIME_ZONE_INFORMATION tz_info {};
	if (GetDynamicTimeZoneInformation(&tz_info) != TIME_ZONE_ID_INVALID) {
		const WCHAR *name = result.tm.tm_isdst > 0 ? tz_info.DaylightName : tz_info.StandardName;
		const int buf_size = static_cast<int>(std::size(result.zone_name));
		if (WideCharToMultiByte(CP_UTF8, 0, name, -1, result.zone_name, buf_size, nullptr, nullptr) == 0) {
			result.zone_name[0] = '\0';
		}
	}
#endif
	return result;
}

#ifndef _WIN32
bool Time::nanosleepFor(struct timespec timeout) noexcept
{
	int res = clock_nanosleep(CLOCK_MONOTONIC, 0, &timeout, nullptr);

	if (res != 0) [[unlikely]] {
		assert(res == EINTR);
		return false;
	}

	return true;
}
#else
namespace
{

struct TimerHandle {
	~TimerHandle()
	{
		if (handle) {
			CloseHandle(handle);
		}
	}

	HANDLE handle = nullptr;
};

// Created on thread-first call to `nanosleepFor()`, destroyed automatically at thread exit
static thread_local TimerHandle t_sleep_timer;

} // namespace

bool Time::nanosleepFor(struct timespec timeout) noexcept
{
	TimerHandle &timer = t_sleep_timer;

	if (!timer.handle) [[unlikely]] {
		// XXX: `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` is available on windows 10 1803 and higher,
		// should we do a fallback for older versions? Not decided which is the minimal supported.
		timer.handle = CreateWaitableTimerExW(nullptr, nullptr,
			CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

		if (!timer.handle) [[unlikely]] {
			// Make it really noticeable
			Log::fatal(
				"'Time::nanosleepFor()' could not create a timer with 'CREATE_WAITABLE_TIMER_HIGH_RESOLUTION'.\n"
				"This flag is supported from Windows 10 1803, and there is no fallback path implemented.\n"
				"Most likely you are running an older Windows version.");
			debug::bugFound("Can't create high-resolution timer for 'Time::nanosleepFor()'");
		}
	}

	LARGE_INTEGER li;
	// Convert into units of 100ns.
	// Negative value means relative timeout.
	li.QuadPart = -int64_t(timeout.tv_sec) * 10'000'000 - int64_t(timeout.tv_nsec) / 100;

	[[maybe_unused]] BOOL set_res = SetWaitableTimer(timer.handle, &li, 0, nullptr, nullptr, FALSE);
	assert(set_res == TRUE);

	[[maybe_unused]] DWORD wait_res = WaitForSingleObject(timer.handle, INFINITE);
	assert(wait_res == WAIT_OBJECT_0);

	return true;
}
#endif

} // namespace voxen::os
