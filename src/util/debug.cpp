#include <voxen/util/debug.hpp>

#ifndef _WIN32
	#include <cxxabi.h>
#endif

namespace voxen
{

DebugUtils::DemanglePtr DebugUtils::demangle(const char *name) noexcept
{
#ifndef _WIN32
	char *ptr = abi::__cxa_demangle(name, nullptr, nullptr, nullptr);
#else
	// TODO (Svenny): add demangling for windows
	char *ptr = strdup(name);
#endif
	return DemanglePtr(ptr, &std::free);
}

} // namespace voxen
