#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace extras
{

// Zero-overhead type-safe pimpl idiom implementation. Size and alignment of implementation
// are the only information seen externally. Size/alignment are checked against actual ones
// at compile time, so it's impossible to have storage overflow. It's advised to allocate a
// bit more storage than is actually used to accomodate for future extensions. Usage:
//
// `foo.hpp`
// class Foo {
// ...
// private:
//     struct Impl;
//     extras::pimpl<Impl, 64, 8> m_impl;
//     Impl &impl();
// };
//
// `foo.cpp`
// struct Foo::Impl {
//     Impl(arg1, arg2);
//     int val1;
//     int val2;
// };
//
// Foo::Foo() : m_impl(1, 2) {}
// Foo::Impl Foo::impl() { return m_impl.object(); }
//
// int Foo::foo()
// {
//     return impl().val1 + impl().val2;
// }
template<typename T, size_t S, size_t A>
class pimpl {
public:
	template<typename... Args>
	constexpr pimpl(Args&&... args);

	constexpr pimpl(pimpl &&other) noexcept;
	constexpr pimpl(const pimpl &other);
	constexpr pimpl &operator = (pimpl &&other) noexcept;
	constexpr pimpl &operator = (const pimpl &other);
	constexpr ~pimpl() noexcept;

	constexpr T &object() noexcept;
	constexpr const T &object() const noexcept;

private:
	std::aligned_storage_t<S, A> m_storage;
};

template<typename T, size_t S, size_t A> template<typename... Args>
constexpr pimpl<T, S, A>::pimpl(Args&&... args)
{
	static_assert(S >= sizeof(T), "Storage is too small");
	static_assert(A >= alignof(T), "Alignment is too weak");
	new (&m_storage) T(std::forward<Args>(args)...);
}

template<typename T, size_t S, size_t A>
constexpr pimpl<T, S, A>::pimpl(pimpl &&other) noexcept
{
	static_assert(S >= sizeof(T), "Storage is too small");
	static_assert(A >= alignof(T), "Alignment is too weak");
	static_assert(std::is_nothrow_move_constructible_v<T>, "Type must be nothrow move-constructible");
	new (&m_storage) T(std::move(other.object()));
}

template<typename T, size_t S, size_t A>
constexpr pimpl<T, S, A>::pimpl(const pimpl &other)
{
	static_assert(S >= sizeof(T), "Storage is too small");
	static_assert(A >= alignof(T), "Alignment is too weak");
	static_assert(std::is_copy_constructible_v<T>, "Type must be copy-constructible");
	new (&m_storage) T(other);
}

template<typename T, size_t S, size_t A>
constexpr pimpl<T, S, A> &pimpl<T, S, A>::operator = (pimpl &&other) noexcept
{
	static_assert(std::is_nothrow_move_assignable_v<T>, "Type must be nothrow move-assignable");
	object() = std::move(other);
	return *this;
}

template<typename T, size_t S, size_t A>
constexpr pimpl<T, S, A> &pimpl<T, S, A>::operator = (const pimpl &other)
{
	static_assert(std::is_copy_assignable_v<T>, "Type must be copy-assignable");
	object() = other;
	return *this;
}

template<typename T, size_t S, size_t A>
constexpr pimpl<T, S, A>::~pimpl() noexcept
{
	static_assert(std::is_nothrow_destructible_v<T>, "Type must be nothrow destructible");
	object().~T();
}

template<typename T, size_t S, size_t A>
constexpr T &pimpl<T, S, A>::object() noexcept
{
	return *std::launder(reinterpret_cast<T *>(&m_storage));
}

template<typename T, size_t S, size_t A>
constexpr const T &pimpl<T, S, A>::object() const noexcept
{
	return *std::launder(reinterpret_cast<const T *>(&m_storage));
}

}
