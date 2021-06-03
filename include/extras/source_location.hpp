#pragma once

#if __has_include(<source_location>)

#include <source_location>
// Just alias (assume it's stable enough when it becomes part of `std`)
namespace extras { using source_location = std::source_location; }

#else

#if __has_include(<experimental/source_location>)
#include <experimental/source_location>
#endif

#ifdef __clang__
#include <string_view>
#endif

#include <cstddef>
#include <cstdint>

namespace extras
{

#ifdef __clang__
consteval inline size_t findBaseDirOffset(std::string_view str) noexcept
{
	// Assuming there is only one "include" directory in the path
	for (size_t i = 0; i + 6 < str.length(); i++) {
		if (str[i] == 'i' && str[i + 1] == 'n' && str[i + 2] == 'c' && str[i + 3] == 'l' &&
		    str[i + 4] == 'u' && str[i + 5] == 'd' && str[i + 6] == 'e') {
			return i;
		}
	}

	return 0;
}
#endif

// Almost exact copy of `std::source_location` from C++20.
// Provides consistent behaviour until standard one is properly implemented.
class source_location final {
public:

#if defined(__has_builtin) && \
	__has_builtin(__builtin_FILE) && __has_builtin(__builtin_FUNCTION) && \
	__has_builtin(__builtin_LINE) && __has_builtin(__builtin_COLUMN)

	// GCC/Clang-specific implementation. Not using `std::experimental` one even if it is available.
	constexpr static source_location current(const char *file = __builtin_FILE(),
	                                         const char *func = __builtin_FUNCTION(),
	                                         uint_least32_t line = __builtin_LINE(),
	                                         uint_least32_t col = __builtin_COLUMN()) noexcept
	{
		source_location loc;
		loc.m_file = file + FILE_BASE_DIR_OFFSET;
		loc.m_func = func;
		loc.m_line = line;
		loc.m_col = col;
		return loc;
	}

#elif __has_include(<experimental/source_location>)

	// Fallback implementation by almost aliasing `std::experimental::source_location`
	constexpr static source_location current(std::experimental::source_location where =
	                                         std::experimental::source_location::current()) noexcept
	{
		source_location loc;
		loc.m_file = where.file_name() + FILE_BASE_DIR_OFFSET;
		loc.m_func = where.function_name();
		loc.m_line = where.line();
		loc.m_col = where.column();
		return loc;
	}

#else

#error source_location is not supported in this environment

#endif

	constexpr const char *file_name() const noexcept { return m_file; }
	constexpr const char *function_name() const noexcept { return m_func; }
	constexpr uint_least32_t line() const noexcept { return m_line; }
	constexpr uint_least32_t column() const noexcept { return m_col; }

private:
#ifdef __clang__
	// Clang (as of 12.0.0) does not apply `-fmacro-prefix-map` to `__builtin_FILE`
	constexpr inline static size_t FILE_BASE_DIR_OFFSET = findBaseDirOffset(__builtin_FILE());
#else
	constexpr inline static size_t FILE_BASE_DIR_OFFSET = 0;
#endif

	const char *m_file = "unknown";
	const char *m_func = "unknown";
	uint_least32_t m_line = 0;
	uint_least32_t m_col = 0;
};

}

#endif
