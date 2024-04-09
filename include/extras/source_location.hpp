#pragma once

#include <version>

#if defined(__cpp_lib_source_location) && __cpp_lib_source_location >= 201907L

	#include <source_location>
// Just alias (assume it's stable enough when it becomes part of `std`)
namespace extras
{
using source_location = std::source_location;
}

#else

	#include <cstddef>
	#include <cstdint>

namespace extras
{

// Own implementation of `std::source_location` from C++20 (with file/line only).
// Provides consistent behaviour until standard one is stable and widely available.
class source_location final {
public:
	// GCC/Clang-specific implementation. Not using `std::experimental` one even if it is available.
	constexpr static source_location current(const char *file = __builtin_FILE(),
		uint_least32_t line = __builtin_LINE()) noexcept
	{
		source_location loc;
		loc.m_file = file;
		loc.m_line = line;
		return loc;
	}

	constexpr const char *file_name() const noexcept { return m_file; }
	constexpr uint_least32_t line() const noexcept { return m_line; }

private:
	const char *m_file = "unknown";
	uint_least32_t m_line = 0;
};

} // namespace extras

#endif // __cpp_lib_source_location >= 201907L
