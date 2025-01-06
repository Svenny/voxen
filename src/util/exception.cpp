#include <voxen/util/exception.hpp>

#include <fmt/format.h>

namespace voxen
{

// Not defining inline to satisfy -Wweak-vtables
Exception::~Exception() = default;

Exception Exception::fromErrorCode(std::error_code ec, const char *details, Location loc)
{
	return { fmt::format("{} (code [{}:{}] {})", details, ec.category().name(), ec.value(), ec.message()),
		ec.default_error_condition(), loc };
}

Exception Exception::fromError(std::error_condition ec, const char *details, Location loc)
{
	return { fmt::format("{} (cond [{}:{}] {})", details, ec.category().name(), ec.value(), ec.message()), ec, loc };
}

Exception::Exception(std::string what, std::error_condition error, Location loc)
	: m_what(std::move(what)), m_error(error), m_where(loc)
{
	// TODO: this is the best place to capture stacktrace
}

} // namespace voxen
