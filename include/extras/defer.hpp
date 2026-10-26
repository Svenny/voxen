#pragma once

#include <exception>
#include <type_traits>
#include <utility>

// Go-style deferred calls, like `std::experimental::scope_exit` and `scope_fail`
// from Library Fundamentals TS v3 (which MS STL doesn't have). Usage:
//
// void foo(int value)
// {
//     // Runs on any scope exit
//     defer { bar(); };
//     // Runs only when the scope is left by an exception
//     defer_fail { baz(); };
//
//     if (value == 0) {
//         // `bar()` is called here
//         return;
//     } else if (value == 1) {
//         // `baz()`, then `bar()` are called here
//         throw 10;
//     }
//     // `bar()` is called here
// }
//
// Deferred code must be noexcept, it runs from a destructor.

namespace extras::detail
{

template<typename F, bool FAIL_ONLY>
class deferred_call {
public:
	static_assert(std::is_nothrow_invocable_v<F &>, "Deferred code must be noexcept");

	explicit deferred_call(F &&f) noexcept : m_f(std::move(f)) {}

	deferred_call(deferred_call &&) = delete;
	deferred_call(const deferred_call &) = delete;
	deferred_call &operator=(deferred_call &&) = delete;
	deferred_call &operator=(const deferred_call &) = delete;

	~deferred_call() noexcept
	{
		// Compare against the count at construction, not against zero: during stack
		// unwinding a scope can be left normally while an exception is in flight
		if (FAIL_ONLY && std::uncaught_exceptions() <= m_uncaught_exceptions) {
			return;
		}

		m_f();
	}

private:
	F m_f;
	int m_uncaught_exceptions = FAIL_ONLY ? std::uncaught_exceptions() : 0;
};

struct defer_tag {};
struct defer_fail_tag {};

template<typename F>
deferred_call<F, false> operator+(defer_tag, F &&f) noexcept
{
	return deferred_call<F, false>(std::forward<F>(f));
}

template<typename F>
deferred_call<F, true> operator+(defer_fail_tag, F &&f) noexcept
{
	return deferred_call<F, true>(std::forward<F>(f));
}

} // namespace extras::detail

#define EXTRAS_DEFER_CONCAT_IMPL(x, y) x##y
#define EXTRAS_DEFER_CONCAT(x, y) EXTRAS_DEFER_CONCAT_IMPL(x, y)
#define defer \
	auto EXTRAS_DEFER_CONCAT(extras_deferred_call_, __COUNTER__) = extras::detail::defer_tag {} + [&]() noexcept
#define defer_fail \
	auto EXTRAS_DEFER_CONCAT(extras_deferred_call_, __COUNTER__) = extras::detail::defer_fail_tag {} + [&]() noexcept
