#include <voxen/util/exception.hpp>

#include <fmt/format.h>

#include <cstring>

namespace voxen
{

const char* FormattedMessageException::kExceptionAccuresMsg = "Exception accures along creating formatted message";

Exception::Exception(const std::experimental::source_location &loc) : m_where(loc) {
	// TODO: this is the best place to print stack trace
}

ErrnoException::ErrnoException(int code, const std::experimental::source_location &loc)
   : Exception(loc) {
	char buf[1024];
	m_message = fmt::format("Error code {}: {}", code, strerror_r(code, buf, 1024));
}

static thread_local fmt::memory_buffer t_message_buffer;
FormattedMessageException::FormattedMessageException(
	std::string_view format_str,
	const fmt::format_args& format_args,
	const std::experimental::source_location &loc
) : Exception(loc), m_exception_accures(false) {
	try {
		fmt::vformat_to(t_message_buffer, format_str, format_args);
		m_what = std::string(t_message_buffer.data());
	} catch (...) {
		m_exception_accures = true;
	}
}

const char * FormattedMessageException::what() const noexcept
{
	if (m_exception_accures)
		return kExceptionAccuresMsg;
	else
		return m_what.c_str();
}

}
