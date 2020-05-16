#include <voxen/util/exception.hpp>

#include <fmt/format.h>

#include <cstring>

namespace voxen
{

const char* FormattedMessageException::kExceptionOccuredMsg =
      "Exception occured during creating FormattedMessageException, message is lost";

Exception::Exception(const std::experimental::source_location &loc) : m_where(loc) {
	// TODO: this is the best place to print stack trace
}

ErrnoException::ErrnoException(int code, const std::experimental::source_location &loc)
   : Exception(loc) {
	char buf[1024];
	m_message = fmt::format("Error code {}: {}", code, strerror_r(code, buf, 1024));
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
