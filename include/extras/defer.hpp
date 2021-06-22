/* Golang-style defer functionality
 * A modified version of this snippet:
 * https://gist.github.com/p2004a/045726d70a490d12ad62
 *
 * Usage:
 *
 * void foo(int value)
 * {
 *     // Works on any scope exit
 *     defer { bar(); };
 *     // Works only when exiting the scope via exception
 *     defer_fail { baz(); };
 *
 *     if (value == 0) {
 *         // Automatic call to `bar()` here
 *         return;
 *     } else if (value == 1) {
 *         // Automatic call to `baz()` here
 *         // Automatic call to `bar()` here
 *         throw 10;
 *     }
 *     // Automatic call to `bar()` here
 * }
 *
 * NOTE: body of deferred code must be noexcept. Doing potentially
 * throwing operations in destructor is a bad idea anyway.
*/
#pragma once

#include <exception>
#include <utility>

namespace extras
{

template<typename F, bool FAIL_ONLY = false>
class defer_finalizer {
public:
	template<typename T>
	defer_finalizer(T &&f_) noexcept : f(std::forward<T>(f_)) {}

	defer_finalizer(defer_finalizer &&) = delete;
	defer_finalizer(const defer_finalizer &) = delete;
	defer_finalizer &operator = (defer_finalizer &&) = delete;
	defer_finalizer &operator = (const defer_finalizer &) = delete;

	~defer_finalizer() noexcept
	{
		if constexpr (FAIL_ONLY) {
			if (std::uncaught_exceptions() == 0) {
				return;
			}
		}

		f();
	}
private:
	F f;
};

[[maybe_unused]] static struct {
	template<typename F>
	defer_finalizer<F> operator << (F &&f) noexcept
	{
		return defer_finalizer<F>(std::forward<F>(f));
	}

	template<typename F>
	defer_finalizer<F, true> operator >> (F &&f) noexcept
	{
		return defer_finalizer<F, true>(std::forward<F>(f));
	}
} deferrer;

}

#define DEFER_TOKENPASTE(x, y) x ## y
#define DEFER_TOKENPASTE2(x, y) DEFER_TOKENPASTE(x, y)
#define defer auto DEFER_TOKENPASTE2(__deferred_lambda_call, __COUNTER__) = extras::deferrer << [&]() noexcept
#define defer_fail auto DEFER_TOKENPASTE2(__deferred_lambda_call, __COUNTER__) = extras::deferrer >> [&]() noexcept
