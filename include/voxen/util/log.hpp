#pragma once

#include <voxen/visibility.hpp>

#include <extras/enum_utils.hpp>
#include <extras/source_location.hpp>

#include <fmt/core.h>

namespace voxen
{

class VOXEN_API Log {
public:
	// Log levels are defined by increasing severity - it's valid to compare them as integers
	enum class Level : int {
		/* Information about implementation details of some specific action (e.g. values of variables on each
		 * iteration of a loop in some algorithm). There should generally be no logging of this level in the
		 * production code as it may clutter the log and cause significant performance overhead, while its
		 * scope of applicability is too narrow (usually only useful during one debugging session). */
		Trace,
		/* A general information about the low-level program workflow (e.g when entering/exiting a
		 * specific function). Entries of this type may be used in places where expectations of a failure
		 * are relatively higher (e.g in operations involving file or network I/O). */
		Debug,
		/* A general information about the high-level program workflow (e.g. when transitioning between high-level
		 * states or when entering/exiting some large or long-running functions). Entries of this type should give a
		 * general understanding of what happens in the program. */
		Info,
		/* An error happened, but the current action can still be completed, though with some negative impact
		 * (e.g. extra performance overhead or partially missing data). */
		Warn,
		/* An error happened which makes completing the current action impossible (e.g. file not found
		 * during texture loading) but does not require immediate program termination. */
		Error,
		/* An error happened which makes further program execution impossible (e.g. segfault). In most
		 * cases this implies (more probably) a bug in the code or (less probably) some external
		 * environment malfunction, such as going out of memory, losing access to critical files,
		 * driver or hardware failure etc. */
		Fatal,

		Off // Not actually a logging level, use it with `setLevel` to disable logging completely
	};

	template<typename... Args>
	static void log(Level level, extras::source_location where, std::string_view format_str, Args &&...args) noexcept
	{
		if (!willBeLogged(level)) {
			return;
		}
		doLog(level, where, format_str, fmt::make_format_args(args...));
	}

	// Returns the current logging level.
	// Initially it is set to `Trace` regardless of build type (i.e. everything is logged).
	static Level level() noexcept { return m_current_level; }
	// Changes the current logging level
	static void setLevel(Level level) noexcept;
	// Returns whether logging with the given level will ultimately output something
	static bool willBeLogged(Level level) noexcept { return level >= m_current_level; }

private:
	Log() = delete;

	static Level m_current_level;

	static void doLog(Level level, extras::source_location where, std::string_view format_str,
		fmt::format_args format_args) noexcept;

public:
	// Yes, a very long code generation macro
#define LOG_MAKE_FUNCTIONS(name, level) \
	template<typename T1> \
	constexpr static void name(T1 &&arg1, extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1)); \
	} \
	template<typename T1, typename T2> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2)); \
	} \
	template<typename T1, typename T2, typename T3> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4, typename T5> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4), std::forward<T5>(arg5)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, T6 &&arg6, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4), std::forward<T5>(arg5), std::forward<T6>(arg6)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, T6 &&arg6, T7 &&arg7, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4), std::forward<T5>(arg5), std::forward<T6>(arg6), std::forward<T7>(arg7)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, T6 &&arg6, T7 &&arg7, T8 &&arg8, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4), std::forward<T5>(arg5), std::forward<T6>(arg6), std::forward<T7>(arg7), \
			std::forward<T8>(arg8)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, \
		typename T9> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, T6 &&arg6, T7 &&arg7, T8 &&arg8, \
		T9 &&arg9, extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4), std::forward<T5>(arg5), std::forward<T6>(arg6), std::forward<T7>(arg7), \
			std::forward<T8>(arg8), std::forward<T9>(arg9)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, \
		typename T9, typename T10> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, T6 &&arg6, T7 &&arg7, T8 &&arg8, \
		T9 &&arg9, T10 &&arg10, extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4), std::forward<T5>(arg5), std::forward<T6>(arg6), std::forward<T7>(arg7), \
			std::forward<T8>(arg8), std::forward<T9>(arg9), std::forward<T10>(arg10)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, \
		typename T9, typename T10, typename T11> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, T6 &&arg6, T7 &&arg7, T8 &&arg8, \
		T9 &&arg9, T10 &&arg10, T11 &&arg11, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4), std::forward<T5>(arg5), std::forward<T6>(arg6), std::forward<T7>(arg7), \
			std::forward<T8>(arg8), std::forward<T9>(arg9), std::forward<T10>(arg10), std::forward<T11>(arg11)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, \
		typename T9, typename T10, typename T11, typename T12> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, T6 &&arg6, T7 &&arg7, T8 &&arg8, \
		T9 &&arg9, T10 &&arg10, T11 &&arg11, T12 &&arg12, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4), std::forward<T5>(arg5), std::forward<T6>(arg6), std::forward<T7>(arg7), \
			std::forward<T8>(arg8), std::forward<T9>(arg9), std::forward<T10>(arg10), std::forward<T11>(arg11), \
			std::forward<T12>(arg12)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, \
		typename T9, typename T10, typename T11, typename T12, typename T13> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, T6 &&arg6, T7 &&arg7, T8 &&arg8, \
		T9 &&arg9, T10 &&arg10, T11 &&arg11, T12 &&arg12, T13 &&arg13, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4), std::forward<T5>(arg5), std::forward<T6>(arg6), std::forward<T7>(arg7), \
			std::forward<T8>(arg8), std::forward<T9>(arg9), std::forward<T10>(arg10), std::forward<T11>(arg11), \
			std::forward<T12>(arg12), std::forward<T13>(arg13)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, \
		typename T9, typename T10, typename T11, typename T12, typename T13, typename T14> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, T6 &&arg6, T7 &&arg7, T8 &&arg8, \
		T9 &&arg9, T10 &&arg10, T11 &&arg11, T12 &&arg12, T13 &&arg13, T14 &&arg14, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4), std::forward<T5>(arg5), std::forward<T6>(arg6), std::forward<T7>(arg7), \
			std::forward<T8>(arg8), std::forward<T9>(arg9), std::forward<T10>(arg10), std::forward<T11>(arg11), \
			std::forward<T12>(arg12), std::forward<T13>(arg13), std::forward<T14>(arg14)); \
	} \
	template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, \
		typename T9, typename T10, typename T11, typename T12, typename T13, typename T14, typename T15> \
	constexpr static void name(T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, T6 &&arg6, T7 &&arg7, T8 &&arg8, \
		T9 &&arg9, T10 &&arg10, T11 &&arg11, T12 &&arg12, T13 &&arg13, T14 &&arg14, T15 &&arg15, \
		extras::source_location where = extras::source_location::current()) noexcept \
	{ \
		log(level, where, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), \
			std::forward<T4>(arg4), std::forward<T5>(arg5), std::forward<T6>(arg6), std::forward<T7>(arg7), \
			std::forward<T8>(arg8), std::forward<T9>(arg9), std::forward<T10>(arg10), std::forward<T11>(arg11), \
			std::forward<T12>(arg12), std::forward<T13>(arg13), std::forward<T14>(arg14), std::forward<T15>(arg15)); \
	}

	LOG_MAKE_FUNCTIONS(trace, Level::Trace)
	LOG_MAKE_FUNCTIONS(debug, Level::Debug)
	LOG_MAKE_FUNCTIONS(info, Level::Info)
	LOG_MAKE_FUNCTIONS(warn, Level::Warn)
	LOG_MAKE_FUNCTIONS(error, Level::Error)
	LOG_MAKE_FUNCTIONS(fatal, Level::Fatal)

#undef LOG_MAKE_FUNCTIONS
};

} // namespace voxen

namespace extras
{

template<>
std::string_view enum_name(voxen::Log::Level value) noexcept;

}
