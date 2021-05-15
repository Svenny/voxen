#include <voxen/util/log.hpp>

#include <cstdio>

namespace voxen
{

Log::Level Log::m_current_level = BuildConfig::kIsDebugBuild ? Log::Level::Debug : Log::Level::Info;

static const char *const kLogLevelString[] = {
   "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static thread_local fmt::memory_buffer t_message_buffer;

void Log::doLog (Level level, std::experimental::source_location where,
                 std::string_view format_str, fmt::format_args format_args) noexcept {
	try {
		fmt::vformat_to(t_message_buffer, format_str, format_args);
		fprintf (stdout, "[%s:%u][%s] ", where.file_name(), where.line (), kLogLevelString[int(level)]);
		fwrite(t_message_buffer.data(), t_message_buffer.size(), sizeof(fmt::memory_buffer::value_type), stdout);
		fputc ('\n', stdout);
		if (level >= Level::Warn) {
			// It's probably desired to see warnings as early as possible, so flush the output buffer
			fflush(stdout);
		}
	}
	catch (const fmt::format_error &err) {
		fprintf(stderr, "[%s:%u][%s] Caught fmt::format_error when trying to log: %s\n",
		        where.file_name(), where.line(), kLogLevelString[int(Level::Warn)], err.what());
	}
	catch (const std::bad_alloc &err) {
		fprintf(stderr, "[%s:%u][%s] Caught std::bad_alloc when trying to log: %s\n",
		        where.file_name(), where.line(), kLogLevelString[int(Level::Warn)], err.what());
	}
	catch (...) {
		fprintf(stderr, "[%s:%u][%s] Caught unknown exception when trying to log\n",
		        where.file_name(), where.line(), kLogLevelString[int(Level::Warn)]);
	}
	t_message_buffer.clear();
}

void Log::setLevel(Level level) noexcept {
	m_current_level = level;
	info("Changing log level to [{}]", kLogLevelString[int(level)]);
}

}
