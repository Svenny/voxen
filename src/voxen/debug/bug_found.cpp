#include <voxen/debug/bug_found.hpp>

#include <voxen/util/log.hpp>

#include <bit>
#include <cassert>
#include <cstdint>
#include <stacktrace>

namespace voxen::debug
{

void bugFound(std::string_view message, extras::source_location where)
{
	Log::fatal("----[ BUG FOUND ]----", where);
	Log::fatal("Please fill an issue on https://github.com/Svenny/voxen", where);
	Log::fatal("and attach this log output. Some related information:", where);
	Log::fatal("Explanation message: {}", message, where);

	// Skip the frame of this function
	auto trace = std::stacktrace::current(1);
	Log::fatal("Stack trace:", where);

	for (size_t i = 0; const std::stacktrace_entry &entry : trace) {
		if (!entry) {
			// libstdc++ can add an invalid terminating entry
			continue;
		}

		if (entry.source_file().empty()) {
			// Native handle is integer in libstdc++ but pointer in MSVC STL
			Log::fatal("#{} {} [{:#x}]", i, entry.description(), std::bit_cast<uintptr_t>(entry.native_handle()), where);
		} else {
			Log::fatal("#{} {} at {}:{}", i, entry.description(), entry.source_file(), entry.source_line(), where);
		}

		i++;
	}

	// TODO: save crash dump
	// TODO: initiate emergency game save?

	Log::fatal("----[ ABORTING VOXEN ]----");

	// Try to break into debugger if it's present
	// TODO: use debugger detection mechanism, otherwise save crash dump and abort
	__builtin_debugtrap();
	abort();
}

} // namespace voxen::debug
