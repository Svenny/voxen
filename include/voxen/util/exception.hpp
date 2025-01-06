#pragma once

#include <voxen/visibility.hpp>

#include <extras/source_location.hpp>

#include <exception>
#include <string>
#include <system_error>

namespace voxen
{

// Base exception class for all exceptions thrown by the engine.
// Note, however, that external libraries throw their own exception types,
// and sometimes those can go through the engine code and end up in your
// (external caller) stack frames. Usually these cases are catastrophic
// failures anyway, and it's not possible to meaningfully react on them.
//
// It is recommended to use this class direrctly and not subclass it
// for specific subsystems unless you can pass some valuable additional
// information. For most purposes, including reacting on the error kind,
// having `std::error_condition` stored (`error()` method) is enough.
//
// Throwing is slow, and this object is not cheap to construct either.
// Therefore you are strongly advised to avoid throwing exceptions for, umm, non-exceptional
// results, i.e. where failures are a relatively "normal" part of the workflow. Sometimes
// that makes implementing two interfaces for the same operation sensible - one that throws
// and the other that returns some status object like `bool`, `std::optional` or `cpp::result`.
// For example, that's how most of `std::filesystem` is implemented.
class VOXEN_API Exception : public std::exception {
public:
	using Location = extras::source_location;

	Exception() = delete;
	Exception(Exception &&) = default;
	Exception(const Exception &) = default;
	Exception &operator=(Exception &&) = default;
	Exception &operator=(const Exception &) = default;
	~Exception() override;

	const char *what() const noexcept override { return m_what.c_str(); }
	const std::error_condition &error() const noexcept { return m_error; }
	// Source location where the exception was thrown.
	// You can either print it manually or pass it to `Log` functions
	// to make logs appear as if they were made in that exact location.
	const Location &where() const noexcept { return m_where; }

	// Construct exception from `std::error_code`. Use this when directly
	// wrapping error code returned from an external library/platform call.
	//
	// `what()` string will be formatted like this:
	// "<details> (code [<ec.category>:<ec>] <ec.message>)"
	static Exception fromErrorCode(std::error_code ec, const char *details, Location loc = Location::current());

	// Construct exception from `std::error_condition`.
	// Generally you should use this form and combine it with `VoxenErrc`, `std::errc`
	// or something returned from mapping `std::error_code` to its default error condition.
	//
	// `what()` string will be formatted like this:
	// "<details> (cond [<ec.category>:<ec>] <ec.message>)"
	static Exception fromError(std::error_condition ec, const char *details, Location loc = Location::current());

protected:
	Exception(std::string what, std::error_condition error, Location loc);

private:
	std::string m_what;
	std::error_condition m_error;
	Location m_where;
};

} // namespace voxen
