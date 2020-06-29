#include <voxen/util/exception.hpp>

#include <voxen/util/log.hpp>

#include <fmt/format.h>

#include <cstring>

namespace voxen
{

const char* FormattedMessageException::kExceptionOccuredMsg =
      "Exception occured during creating FormattedMessageException, message is lost";

Exception::Exception(const std::experimental::source_location &loc) : m_where(loc) {
	// TODO: this is the best place to print stack trace
}

ErrnoException::ErrnoException(int code, const char *api, const std::experimental::source_location &loc)
   : Exception(loc) {
	// TODO: not exception-safe
	char buf[1024];
	const char *description = strerror_r(code, buf, 1024);
	if (api) {
		Log::error("{} failed with error code {} ({})", api, code, description, loc);
		m_message = fmt::format("Error code {}: {}", code, description);
	} else {
		m_message = fmt::format("Error code {}: {}", code, description);
	}
}

FormattedMessageException::FormattedMessageException(
	std::string_view format_str,
	const fmt::format_args& format_args,
	const std::experimental::source_location &loc
) : Exception(loc), m_exception_occured(false) {
	try {
		m_what = fmt::vformat(format_str, format_args);
	} catch (...) {
		m_exception_occured = true;
	}
}

const char * FormattedMessageException::what() const noexcept
{
	if (m_exception_occured)
		return kExceptionOccuredMsg;
	else
		return m_what.c_str();
}

}
