#include <voxen/util/debug.hpp>

#include <cxxabi.h>

namespace voxen
{

DebugUtils::DemanglePtr DebugUtils::demangle(const char *name) noexcept
{
	char *ptr = abi::__cxa_demangle(name, nullptr, nullptr, nullptr);
	return DemanglePtr(ptr, &std::free);
}

}
