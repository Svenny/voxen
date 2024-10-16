#include <voxen/os/process.hpp>

#ifndef _WIN32
	#include <unistd.h>
#else
	#include <Windows.h>
	#include <processthreadsapi.h>
#endif

namespace voxen::os
{

int32_t Process::getProcessId() noexcept
{
#ifndef _WIN32
	return getpid();
#else
	return static_cast<int32_t>(GetCurrentProcessId());
#endif
}

int32_t Process::getThreadId() noexcept
{
#ifndef _WIN32
	return gettid();
#else
	return static_cast<int32_t>(GetCurrentThreadId());
#endif
}

} // namespace voxen::os
