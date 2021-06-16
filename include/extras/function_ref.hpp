#pragma once

#include <functional>
#include <type_traits>

namespace extras
{

namespace detail
{

// Not using a static method of `function_ref` because of bug in
// in Clang (checked on 12.0.0) which always discards `noexcept`
template<typename T, typename R, bool NX, typename... Args>
inline R fnref_do_call(Args... args, void *object) noexcept(NX)
{
	if constexpr (std::is_function_v<T>) {
		T *fn = reinterpret_cast<T *>(object);
		return fn(std::forward<Args>(args)...);
	} else {
		T *obj = reinterpret_cast<T *>(object);
		return obj->operator ()(std::forward<Args>(args)...);
	}
}

}

template<typename R>
class function_ref;

// A lightweight but non-owning alternative to `std::function`.
// "Reference" semantics - it can't be empty (unlike `std::function`).
template<typename R, bool NX, typename... Args>
class function_ref<R(Args...) noexcept(NX)> final {
public:
	template<typename T>
	constexpr explicit function_ref(T &object) noexcept
		: m_object(reinterpret_cast<void *>(std::addressof(object))), m_caller(detail::fnref_do_call<T, R, NX, Args...>)
	{}

	// Conversion casting away noexcept specifier
	template<typename T = void> requires(!NX)
	constexpr function_ref(function_ref<R(Args...) noexcept> &&other) noexcept
		: m_object(other.m_object), m_caller(other.m_caller)
	{}

	// Conversion casting away noexcept specifier
	template<typename T = void> requires(!NX)
	constexpr function_ref(const function_ref<R(Args...) noexcept> &other) noexcept
		: m_object(other.m_object), m_caller(other.m_caller)
	{}

	function_ref() = delete;
	function_ref(function_ref &&) = default;
	function_ref(const function_ref &) = default;
	function_ref &operator = (function_ref &&) = default;
	function_ref &operator = (const function_ref &) = default;
	~function_ref() = default;

	R operator()(Args... args) const noexcept(NX) { return m_caller(std::forward<Args>(args)..., m_object); }

private:
	void *m_object = nullptr;
	R (&m_caller)(Args..., void *) noexcept(NX);

	// Enable converting copy ctor (noexcept -> !noexcept cast)
	friend class function_ref<R(Args...) noexcept(false)>;
};

namespace detail
{

// Adapted from libstdc++'s deduction guides for `std::function`
template<typename>
struct fnref_guide_helper {};

template<typename R, typename T, bool NX, typename... Args>
struct fnref_guide_helper<R(T::*)(Args...) noexcept(NX)> {
	using type = R(Args...) noexcept(NX);
};

template<typename R, typename T, bool NX, typename... Args>
struct fnref_guide_helper<R(T::*)(Args...) & noexcept(NX)> {
	using type = R(Args...) noexcept(NX);
};

template<typename R, typename T, bool NX, typename... Args>
struct fnref_guide_helper<R(T::*)(Args...) const noexcept(NX)> {
	using type = R(Args...) noexcept(NX);
};

template<typename R, typename T, bool NX, typename... Args>
struct fnref_guide_helper<R(T::*)(Args...) const & noexcept(NX)> {
	using type = R(Args...) noexcept(NX);
};

}

// Deduction guides
template<typename R, bool NX, typename... Args>
function_ref(R(*)(Args...) noexcept(NX)) -> function_ref<R(Args...) noexcept(NX)>;
template<typename T, typename S = typename detail::fnref_guide_helper<decltype(&T::operator())>::type>
function_ref(T) -> function_ref<S>;

}
