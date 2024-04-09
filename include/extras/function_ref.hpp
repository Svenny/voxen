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
		return obj->operator()(std::forward<Args>(args)...);
	}
}

// This tag is used to disallow creating `function_ref` managing
// another `function_ref` which leads to unwanted recursion
struct function_ref_tag {};

} // namespace detail

template<typename R>
class function_ref;

// A lightweight but non-owning alternative to `std::function`.
template<typename R, bool NX, typename... Args>
class function_ref<R(Args...) noexcept(NX)> final : public detail::function_ref_tag {
public:
	template<typename T>
	constexpr function_ref(T &&object) noexcept
		requires(!std::is_base_of_v<detail::function_ref_tag, std::remove_reference_t<T>>)
		: m_object(reinterpret_cast<void *>(std::addressof(object)))
		, m_caller(detail::fnref_do_call<std::remove_reference_t<T>, R, NX, Args...>)
	{}

	template<typename T>
	constexpr function_ref(T *object) noexcept
		requires(!std::is_base_of_v<detail::function_ref_tag, T>)
		: m_object(reinterpret_cast<void *>(object)), m_caller(detail::fnref_do_call<T, R, NX, Args...>)
	{}

	// Conversion casting away noexcept specifier
	template<typename T = void>
		requires(!NX)
	constexpr function_ref(function_ref<R(Args...) noexcept> &&other) noexcept
		: m_object(other.m_object), m_caller(other.m_caller)
	{}

	// Conversion casting away noexcept specifier
	template<typename T = void>
		requires(!NX)
	constexpr function_ref(const function_ref<R(Args...) noexcept> &other) noexcept
		: m_object(other.m_object), m_caller(other.m_caller)
	{}

	constexpr function_ref() = default;
	constexpr function_ref(function_ref &&) = default;
	constexpr function_ref(const function_ref &) = default;
	constexpr function_ref &operator=(function_ref &&) = default;
	constexpr function_ref &operator=(const function_ref &) = default;
	constexpr ~function_ref() = default;

	constexpr explicit operator bool() const noexcept { return m_object != nullptr; }
	constexpr R operator()(Args... args) const noexcept(NX) { return m_caller(std::forward<Args>(args)..., m_object); }

private:
	void *m_object = nullptr;
	R (*m_caller)(Args..., void *) noexcept(NX) = nullptr;

	// Enable converting copy ctor (noexcept -> !noexcept cast)
	friend class function_ref<R(Args...) noexcept(false)>;
};

namespace detail
{

// Adapted from libstdc++'s deduction guides for `std::function`
template<typename>
struct fnref_guide_helper {};

template<typename R, typename T, bool NX, typename... Args>
struct fnref_guide_helper<R (T::*)(Args...) noexcept(NX)> {
	using type = R(Args...) noexcept(NX);
};

template<typename R, typename T, bool NX, typename... Args>
struct fnref_guide_helper<R (T::*)(Args...) & noexcept(NX)> {
	using type = R(Args...) noexcept(NX);
};

template<typename R, typename T, bool NX, typename... Args>
struct fnref_guide_helper<R (T::*)(Args...) const noexcept(NX)> {
	using type = R(Args...) noexcept(NX);
};

template<typename R, typename T, bool NX, typename... Args>
struct fnref_guide_helper<R (T::*)(Args...) const & noexcept(NX)> {
	using type = R(Args...) noexcept(NX);
};

} // namespace detail

// Deduction guides
template<typename R, bool NX, typename... Args>
function_ref(R (*)(Args...) noexcept(NX)) -> function_ref<R(Args...) noexcept(NX)>;
template<typename T, typename S = typename detail::fnref_guide_helper<decltype(&T::operator())>::type>
function_ref(T) -> function_ref<S>;

} // namespace extras
