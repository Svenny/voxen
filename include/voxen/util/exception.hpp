#pragma once

#include <exception>
#include <string>

#include <extras/source_location.hpp>

#include <fmt/core.h>

namespace voxen
{

class Exception : public std::exception {
public:
	explicit Exception(extras::source_location loc = extras::source_location::current()) noexcept;
	virtual ~Exception() = default;

	extras::source_location where() const noexcept { return m_where; }

protected:
	extras::source_location m_where;
};

class MessageException : public Exception {
public:
	explicit MessageException(const char *msg = "", extras::source_location loc = extras::source_location::current())
		noexcept : Exception(loc), m_what(msg) {}
	virtual ~MessageException() = default;

	virtual const char *what() const noexcept override { return m_what; }

protected:
	const char *m_what;
};

class FormattedMessageException : public Exception {
public:
	explicit FormattedMessageException(std::string_view format_str, const fmt::format_args &format_args,
		extras::source_location loc = extras::source_location::current()) noexcept;
	virtual ~FormattedMessageException() = default;

	virtual const char *what() const noexcept override;

protected:
	std::string m_what;
	bool m_exception_occured = false;
};

class ErrnoException : public Exception {
public:
	explicit ErrnoException(int code, const char *api = nullptr, extras::source_location loc =
		extras::source_location::current()) noexcept;
	virtual ~ErrnoException() = default;

	virtual const char *what() const noexcept override;
	int code() const noexcept { return m_code; }

protected:
	std::string m_message;
	int m_code;
	bool m_exception_occured = false;
};

}
