/* Golang-style defer functionality
 * A slightly modified version of this snippet:
 * https://gist.github.com/p2004a/045726d70a490d12ad62 */
#pragma once

#include <exception>
#include <utility>

namespace extras
{

template<typename F, bool is_fail_only = false>
class defer_finalizer {
	F f;
	bool moved;
public:
	template<typename T>
	defer_finalizer(T &&f_) : f(std::forward<T>(f_)), moved(false) {}

	defer_finalizer(const defer_finalizer &) = delete;

	defer_finalizer(defer_finalizer &&other) : f(std::move(other.f)), moved(other.moved) {
		other.moved = true;
	}

	~defer_finalizer() noexcept(is_fail_only) {
		if constexpr (is_fail_only) {
			if (std::uncaught_exceptions() == 0)
				return;
		}
		if (!moved) f();
	}
};

[[maybe_unused]] static struct {
	template<typename F>
	defer_finalizer<F> operator<<(F &&f) {
		return defer_finalizer<F>(std::forward<F>(f));
	}
} deferrer;

[[maybe_unused]] static struct {
	template<typename F>
	defer_finalizer<F, true> operator<<(F &&f) {
		return defer_finalizer<F, true>(std::forward<F>(f));
	}
} fail_deferrer;

}

#define DEFER_TOKENPASTE(x, y) x ## y
#define DEFER_TOKENPASTE2(x, y) DEFER_TOKENPASTE(x, y)
#define defer auto DEFER_TOKENPASTE2(__deferred_lambda_call, __COUNTER__) = extras::deferrer << [&]
#define defer_fail auto DEFER_TOKENPASTE2(__deferred_lambda_call, __COUNTER__) = extras::fail_deferrer << [&]
