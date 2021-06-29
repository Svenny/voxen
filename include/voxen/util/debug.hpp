#pragma once

#include <memory>

namespace voxen
{

class DebugUtils final {
public:
	using DemanglePtr = std::unique_ptr<char[], decltype(&std::free)>;

	// Static class, no instances of it are allowed
	DebugUtils() = delete;

	// Demangle C++ type name. Returns null pointer in case of failure.
	static DemanglePtr demangle(const char *name) noexcept;

	// Demangle the name of provided template argument
	template<typename T>
	static DemanglePtr demangle() noexcept { return demangle(typeid(T).name()); }
};

}
