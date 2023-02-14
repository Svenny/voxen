#pragma once

#include <extras/source_location.hpp>

#include <exception>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace voxen
{

class Exception : public std::exception {
public:
	using Location = extras::source_location;

	Exception() = delete;
	Exception(Exception &&) = default;
	Exception(const Exception &) = default;
	Exception &operator = (Exception &&) = default;
	Exception &operator = (const Exception &) = default;
	~Exception() override = default;

	const char *what() const noexcept override;
	const std::error_condition &error() const noexcept { return m_error; }
	const Location &where() const noexcept { return m_where; }

	static Exception fromError(std::error_condition error, const char *what = nullptr,
	                           Location loc = Location::current()) noexcept;
	static Exception fromFailedCall(std::error_condition error, std::string_view api,
	                                Location loc = Location::current());

protected:
	Exception(std::variant<const char *, std::string> what, std::error_condition error, Location loc) noexcept;

private:
	std::variant<const char *, std::string> m_what;
	std::error_condition m_error;
	Location m_where;
};

}
