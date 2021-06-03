#include <voxen/util/exception.hpp>

#include <voxen/util/log.hpp>

#include <fmt/format.h>

#include <cstring>

namespace voxen
{


Exception::Exception(extras::source_location loc) noexcept : m_where(loc)
{
	// TODO: this is the best place to print stack trace
}

FormattedMessageException::FormattedMessageException(std::string_view format_str, const fmt::format_args &format_args,
	extras::source_location loc) noexcept : Exception(loc)
{
	try {
		m_what = fmt::vformat(format_str, format_args);
	} catch (...) {
		m_exception_occured = true;
	}
}

const char *FormattedMessageException::what() const noexcept
{
	constexpr static char EXCEPTION_OCCURED_MSG[] =
		"Exception occured during creating FormattedMessageException, message is lost";

	if (m_exception_occured) {
		return EXCEPTION_OCCURED_MSG;
	}

	return m_what.c_str();
}

ErrnoException::ErrnoException(int code, const char *api, extras::source_location loc) noexcept
	: Exception(loc), m_code(code)
{
	try {
		char buf[1024];
		const char *description = strerror_r(code, buf, 1024);
		if (api) {
			Log::error("{} failed with error code {} ({})", api, code, description, loc);
			m_message = fmt::format("Error code {}: {}", code, description);
		} else {
			m_message = fmt::format("Error code {}: {}", code, description);
		}
	} catch (...) {
		m_exception_occured = true;
	}
}

const char *ErrnoException::what() const noexcept
{
	constexpr static char EXCEPTION_OCCURED_MSG[] =
		"Exception occured during creating ErrnoException, message is lost";

	if (m_exception_occured) {
		return EXCEPTION_OCCURED_MSG;
	}

	return m_message.c_str();
}

}
