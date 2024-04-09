#include <voxen/util/log.hpp>

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>

#include <unistd.h>

#include <cassert>
#include <cstdio>

namespace voxen
{

Log::Level Log::m_current_level = Log::Level::Trace;

// Thread-local "waterline cache" buffer to avoid allocations on each print.
// FMT can still do temporary internal allocations but we can't control that.
static thread_local fmt::memory_buffer t_message_buffer;

static fmt::text_style styleForLevel(Log::Level level) noexcept
{
	fmt::text_style style;

	switch (level) {
	case Log::Level::Trace:
		style |= fmt::fg(fmt::color::wheat);
		break;
	case Log::Level::Debug:
		style |= fmt::fg(fmt::color::light_sea_green);
		break;
	case Log::Level::Info:
		style |= fmt::fg(fmt::color::green);
		break;
	case Log::Level::Warn:
		style |= fmt::fg(fmt::color::yellow);
		break;
	case Log::Level::Error:
		style |= fmt::fg(fmt::color::red);
		break;
	case Log::Level::Fatal:
		style = fmt::emphasis::bold | fmt::fg(fmt::color::white) | fmt::bg(fmt::color::red);
		break;
	case Log::Level::Off:
		break;
	} // No `default` to make `-Werror -Wswitch` protection work

	return style;
}

void Log::doLog(Level level, extras::source_location where, std::string_view format_str,
	fmt::format_args format_args) noexcept
{
	// Prepare formatted text (or failure message) to be logged
	std::string_view text;

	try {
		auto &msgbuf = t_message_buffer;

		try {
			msgbuf.clear();
			fmt::vformat_to(std::back_inserter(msgbuf), format_str, format_args);
		}
		catch (const fmt::format_error &err) {    // This is the only practically expected error
			level = std::max(level, Level::Error); // Raise log level to at least Error
			msgbuf.clear();                        // Clear the buffer, it could be partially filled by try-code
			fmt::format_to(std::back_inserter(msgbuf), "Caught fmt::format_error when trying to log: {}", err.what());
		}
		catch (const std::bad_alloc &err) {
			level = std::max(level, Level::Error); // Raise log level to at least Error
			msgbuf.clear();                        // Clear the buffer, it could be partially filled by try-code
			// `msgbuf` is large enough to fit this error string inline without hitting `bad_alloc` again
			fmt::format_to(std::back_inserter(msgbuf), "Caught std::bad_alloc when trying to log: {}", err.what());
		}

		text = { msgbuf.data(), msgbuf.size() };
	}
	catch (...) {
		// Getting here means failure in catch clauses or an "unexpected" exception.
		// We can't do anything potentially-throwing here so just use a literal.
		level = std::max(level, Level::Error); // Raise log level to at least Error
		text = "Caught [unknown exception] when trying to log";
	}

	// Now try to print this text
	const auto pid = getpid();
	const auto tid = gettid();

	try {
		// Select the sink (stdout for `X <= Info`, stderr for `X >= Warn`)
		FILE *sink = stdout;
		if (level >= Level::Warn) {
			// Flush `stdout` to not mess the messages when it's directed to same output as `stderr`
			// TODO (Svenny): check if `stdout` and `stderr` target different outputs?
			fflush(stdout);
			sink = stderr;
		}

		fmt::print(sink, "[{:%F %T}][{} {}][{:s}:{:d}][{:s}] {:s}\n", std::chrono::system_clock::now(), pid, tid,
			where.file_name(), where.line(), fmt::styled(extras::enum_name(level), styleForLevel(level)), text);
	}
	catch (const std::system_error &err) {
		// Reaching this clause means printing to `sink` failed for system reasons (bad file?).
		// Retrying here is probably useless if `sink` was `stderr` but might work for `stdout`.
		fflush(stdout);
		// TODO (Svenny): date/time can be printed here too but I'm too lazy to implement it
		fprintf(stderr, "[XXXX-XX-XX XX:XX:XX][%d %d][%s:%u][ERROR] std::system_error when printing log: %s:%d (%s)\n",
			pid, tid, where.file_name(), where.line(), err.code().category().name(), err.code().value(), err.what());
	}
	catch (...) {
		// Reaching this clause means something is deeply broken... but try one last time
		fflush(stdout);
		// TODO (Svenny): date/time can be printed here too but I'm too lazy to implement it
		fprintf(stderr, "[XXXX-XX-XX XX:XX:XX][%d %d][%s:%u][ERROR] Unknown exception when printing log!\n", pid, tid,
			where.file_name(), where.line());
	}
}

void Log::setLevel(Level level) noexcept
{
	m_current_level = level;
	info("Changing log level to [{}]", extras::enum_name(level));
}

} // namespace voxen

namespace extras
{

using voxen::Log;

template<>
std::string_view enum_name(Log::Level value) noexcept
{
	using namespace std::string_view_literals;

	switch (value) {
	case Log::Level::Trace:
		return "TRACE"sv;
	case Log::Level::Debug:
		return "DEBUG"sv;
	case Log::Level::Info:
		return "INFO"sv;
	case Log::Level::Warn:
		return "WARN"sv;
	case Log::Level::Error:
		return "ERROR"sv;
	case Log::Level::Fatal:
		return "FATAL"sv;
	case Log::Level::Off:
		return "OFF"sv;
	} // No `default` to make `-Werror -Wswitch` protection work
}

} // namespace extras
