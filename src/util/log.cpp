#include <voxen/util/log.hpp>

#include <fmt/format.h>

#include <cassert>
#include <cstdio>

namespace voxen
{

Log::Level Log::m_current_level = BuildConfig::kIsDebugBuild ? Log::Level::Debug : Log::Level::Info;

static thread_local fmt::memory_buffer t_message_buffer;

void Log::doLog(Level level, extras::source_location where,
                std::string_view format_str, fmt::format_args format_args) noexcept
{
	try {
		t_message_buffer.clear();
		fmt::vformat_to(t_message_buffer, format_str, format_args);
		std::string_view text(t_message_buffer.data(), t_message_buffer.size());

		FILE *sink = stdout;
		if (level >= Level::Warn) {
			// Flush `stdout` to not mess the messages when it's directed to same output as `stderr`
			fflush(stdout);
			sink = stderr;
		}

		fmt::print(sink, FMT_STRING("[{:s}:{:d}][{:s}] {:s}\n"),
		           where.file_name(), where.line(),
		           extras::enum_name(level), text);
	} catch (const fmt::format_error &err) {
		fflush(stdout);
		fprintf(stderr, "[%s:%u][WARN] Caught fmt::format_error when trying to log: %s\n",
		        where.file_name(), where.line(), err.what());
	} catch (const std::bad_alloc &err) {
		fflush(stdout);
		fprintf(stderr, "[%s:%u][WARN] Caught std::bad_alloc when trying to log: %s\n",
		        where.file_name(), where.line(), err.what());
	} catch (const std::exception &err) {
		fflush(stdout);
		fprintf(stderr, "[%s:%u][WARN] Caught std::exception when trying to log: %s\n",
		        where.file_name(), where.line(), err.what());
	} catch (...) {
		fflush(stdout);
		fprintf(stderr, "[%s:%u][WARN] Caught unknown exception when trying to log\n",
		        where.file_name(), where.line());
	}
}

void Log::setLevel(Level level) noexcept
{
	m_current_level = level;
	info("Changing log level to [{}]", extras::enum_name(level));
}

}

namespace extras
{

using voxen::Log;

template<>
std::string_view enum_name(Log::Level value) noexcept
{
	using namespace std::string_view_literals;

	switch (value) {
		case Log::Level::Trace: return "TRACE"sv;
		case Log::Level::Debug: return "DEBUG"sv;
		case Log::Level::Info: return "INFO"sv;
		case Log::Level::Warn: return "WARN"sv;
		case Log::Level::Error: return "ERROR"sv;
		case Log::Level::Fatal: return "FATAL"sv;
		default:
			assert(false);
			return "UNKNOWN"sv;
	}
}

}
