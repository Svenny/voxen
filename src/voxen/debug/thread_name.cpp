#include <voxen/debug/thread_name.hpp>

#include <cassert>
#include <cstdarg>

#ifndef _WIN32
	#include <pthread.h>
#else
	#define NOMINMAX
	#include <Windows.h>
	#include <processthreadsapi.h>

	#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO {
	DWORD dwType;     // Must be 0x1000
	LPCSTR szName;    // Pointer to name (in user addr space)
	DWORD dwThreadID; // Rhread ID (-1=caller thread)
	DWORD dwFlags;    // Reserved for future use, must be zero
} THREADNAME_INFO;
	#pragma pack(pop)
#endif

namespace voxen::debug
{

// Windows does not specify any length limit.
// Pthreads limit to 16 characters including null terminator.
constexpr static size_t LIMIT = 16;

void setThreadName(std::string_view name)
{
	// Truncate string to the length limit
	char buf[LIMIT] = {};
	strncpy(buf, name.data(), std::min(LIMIT - 1, name.size()));

#ifndef _WIN32
	[[maybe_unused]] int res = pthread_setname_np(pthread_self(), buf);
	assert(res == 0);
#else
	using PfnSetThreadDescription = HRESULT (*)(HANDLE, PCWSTR);

	// Look it up once
	static PfnSetThreadDescription pfn = []() {
		HMODULE kernel32 = LoadLibraryW(L"kernel32.dll");
		if (kernel32) {
			return reinterpret_cast<PfnSetThreadDescription>(GetProcAddress(kernel32, "SetThreadDescription"));
		}
		return (PfnSetThreadDescription) nullptr;
	}();

	if (pfn) {
		// Use `SetThreadDescription` if available (from windows 10 1607 something)
		wchar_t wbuf[LIMIT];
		for (size_t i = 0; i < LIMIT; i++) {
			// String is required to be pure ASCII
			wbuf[i] = static_cast<wchar_t>(buf[i]);
		}

		[[maybe_unused]] HRESULT hr = SetThreadDescription(GetCurrentThread(), wbuf);
		assert(SUCCEEDED(hr));
	} else if (IsDebuggerPresent()) {
		// Otherwise use an arcane exception hack that works just for debugger
		THREADNAME_INFO info {
			.dwType = 0x1000,
			.szName = buf,
			.dwThreadID = static_cast<DWORD>(-1),
			.dwFlags = 0,
		};
		// The debugger will catch this, name the thread and proceed
		RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (const ULONG_PTR *) &info);
	}
#endif
}

void setThreadName(const char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);

	char buf[LIMIT];
	vsnprintf(buf, LIMIT, fmt, arg);
	va_end(arg);

	setThreadName(std::string_view(buf));
}

} // namespace voxen::debug
