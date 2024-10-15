#include <voxen/util/debug.hpp>

#include <voxen/util/log.hpp>

#include <cxxabi.h>
#include <execinfo.h>

#define BACKWARD_HAS_DW 1
#define BACKWARD_HAS_UNWIND 1
#include <backward/backward.hpp>

namespace voxen
{

DebugUtils::DemanglePtr DebugUtils::demangle(const char *name) noexcept
{
	char *ptr = abi::__cxa_demangle(name, nullptr, nullptr, nullptr);
	return DemanglePtr(ptr, &std::free);
}

std::vector<std::string> DebugUtils::stackTrace()
{
	std::vector<std::string> res;
	/*char buf[512];
	char buf2[512];

	unw_cursor_t cursor;
	unw_context_t ctx;

	unw_getcontext(&ctx);
	unw_init_local(&cursor, &ctx);
	while (unw_step(&cursor) > 0) {
		unw_word_t offset;
		unw_proc_info_t proc_info;
		unw_get_proc_name(&cursor, buf, std::size(buf), &offset);
		unw_get_proc_info(&cursor, &proc_info);
		char *demangled = abi::__cxa_demangle(buf, nullptr, nullptr, nullptr);

		if (demangled) {
			snprintf(buf2, std::size(buf2), "(0x%lx) %s+%lld", proc_info.start_ip, demangled, (unsigned long long)offset);
			free(demangled);
		} else {
			snprintf(buf2, std::size(buf2), "(0x%lx) %s+%lld", proc_info.start_ip, buf, (unsigned long long)offset);
		}

		res.emplace_back(buf2);
	}*/

	return res;
}

void DebugUtils::bugFound(const char *text, extras::source_location where)
{
	//auto st = stackTrace();

	Log::fatal("----[ A BUG HAPPENED ]----", where);
	Log::fatal("Please fill an issue on https://github.com/Svenny/voxen", where);
	Log::fatal("and attach this log output. Some related information:", where);
	Log::fatal("Error text: {}", text, where);

	backward::StackTrace st;
	st.load_here();

	backward::Printer pr;
	pr.snippet = false;
	pr.color_mode = backward::ColorMode::automatic;
	pr.address = true;
	pr.object = true;
	pr.reverse = false;
	pr.print(st);

	/*Log::fatal("Stack trace:", where);
	for (size_t i = 0; i < st.size(); i++) {
		Log::fatal("  #{}: {}", i, st[i], where);
	}*/

	Log::fatal("----[ ABORTING VOXEN ]----", where);

	abort();
}

} // namespace voxen
