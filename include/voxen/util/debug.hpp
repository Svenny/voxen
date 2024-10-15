#pragma once

#include <extras/source_location.hpp>

#include <memory>
#include <string>
#include <vector>

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
	static DemanglePtr demangle() noexcept
	{
		return demangle(typeid(T).name());
	}

	static std::vector<std::string> stackTrace();

	// This function should be called when a known bug happens.
	// It prints log message with stacktrace and explanatory text,
	// requests user to create a bugreport and then calls `abort()`.
	[[noreturn]] static void bugFound(const char *text,
		extras::source_location where = extras::source_location::current());
};

} // namespace voxen
