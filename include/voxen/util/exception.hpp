#pragma once

#include <exception>
#include <experimental/source_location>
#include <string>
#include <fmt/format.h>

namespace voxen
{

class Exception : public std::exception {
public:
	explicit Exception(const std::experimental::source_location &loc =
	      std::experimental::source_location::current());
	virtual ~Exception() override = default;

	std::experimental::source_location where() const noexcept { return m_where; }
protected:
	std::experimental::source_location m_where;
};

class MessageException : public Exception {
public:
	explicit MessageException(const char *msg = "", const std::experimental::source_location &loc =
	      std::experimental::source_location::current()) : Exception(loc), m_what(msg) {}
	virtual ~MessageException() override = default;

	virtual const char *what() const noexcept override { return m_what; }
protected:
	const char *m_what;
};

class FormattedMessageException : public Exception {
public:
	explicit FormattedMessageException(std::string_view format_str, const fmt::format_args& format_args,
	      const std::experimental::source_location &loc = std::experimental::source_location::current());
	virtual ~FormattedMessageException() override = default;

	virtual const char *what() const noexcept override;
protected:
	std::string m_what;
	bool m_exception_occured;
	const static char* kExceptionOccuredMsg;
};

class ErrnoException : public Exception {
public:
	explicit ErrnoException(int code, const std::experimental::source_location &loc =
	      std::experimental::source_location::current());
	virtual ~ErrnoException() override = default;

	virtual const char *what() const noexcept override { return m_message.c_str(); }
	int code() const noexcept { return m_code; }
protected:
	int m_code;
	std::string m_message;
};

}
