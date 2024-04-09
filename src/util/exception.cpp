#include <voxen/util/exception.hpp>

#include <voxen/util/error_condition.hpp>

#include <fmt/format.h>

#include <cassert>

namespace voxen
{

const char *Exception::what() const noexcept
{
	if (const std::string *str = std::get_if<std::string>(&m_what); str) {
		return str->c_str();
	}

	// Can directly use `std::get<const char *>()` here but this will trip exception-escape analyzer
	if (const char *const *pstr = std::get_if<const char *>(&m_what); pstr) {
		return *pstr;
	}

	assert(false);
	__builtin_unreachable();
}

Exception Exception::fromError(std::error_condition error, const char *what, Location loc) noexcept
{
	return { what ? what : "see stored error condition", error, loc };
}

Exception Exception::fromFailedCall(std::error_condition error, std::string_view api, Location loc)
{
	assert(api.length() > 0);
	return { fmt::format("call to '{}' failed", api), error, loc };
}

Exception::Exception(std::variant<const char *, std::string> what, std::error_condition error, Location loc) noexcept
	: m_what(std::move(what)), m_error(error), m_where(loc)
{
	// TODO: this is the best place to capture stacktrace
}

} // namespace voxen
