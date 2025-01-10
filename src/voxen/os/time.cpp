#include <voxen/os/time.hpp>

#include <voxen/debug/bug_found.hpp>
#include <voxen/util/log.hpp>

#ifdef _WIN32
	#include <Windows.h>
#endif

#include <cassert>
#include <cstdint>

namespace voxen::os
{

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
