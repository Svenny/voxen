#include <voxen/debug/bug_found.hpp>

#include <voxen/util/log.hpp>

#include <cassert>

#ifndef _WIN32
	#define BACKWARD_HAS_DW 1
#endif

#include <backward/backward.hpp>

namespace voxen::debug
{

void bugFound(std::string_view message, extras::source_location where)
{
	Log::fatal("----[ BUG FOUND ]----", where);
	Log::fatal("Please fill an issue on https://github.com/Svenny/voxen", where);
	Log::fatal("and attach this log output. Some related information:", where);
	Log::fatal("Explanation message: {}", message, where);

	backward::StackTrace st;
	st.load_here();

	backward::Printer pr;
	pr.snippet = false;
	pr.color_mode = backward::ColorMode::automatic;
	pr.address = true;
	pr.object = true;
	pr.reverse = false;
	pr.print(st);

	// TODO: save crash dump
	// TODO: initiate emergency game save?

	Log::fatal("----[ ABORTING VOXEN ]----");

	// Try to break into debugger if it's present
	// TODO: use debugger detection mechanism, otherwise save crash dump and abort
	__builtin_debugtrap();
	abort();
}

} // namespace voxen::debug
