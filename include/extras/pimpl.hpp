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
// Foo::Impl &Foo::impl() { return m_impl.object(); }
//
// int Foo::foo()
// {
//     return impl().val1 + impl().val2;
// }
template<typename T, size_t S, size_t A>
class pimpl {
public:
	// Constructs `T` from `args`. Never takes a single `pimpl` argument, copies and moves go to their own overloads.
	template<typename... Args>
		requires(sizeof...(Args) != 1 || (!std::is_same_v<std::remove_cvref_t<Args>, pimpl> && ...))
	explicit pimpl(Args &&...args);

	pimpl(pimpl &&other) noexcept;
	pimpl(const pimpl &other);
	pimpl &operator=(pimpl &&other) noexcept;
	pimpl &operator=(const pimpl &other);
	~pimpl() noexcept;

	[[clang::always_inline]] T &object() noexcept { return *std::launder(reinterpret_cast<T *>(m_storage)); }

	[[clang::always_inline]] const T &object() const noexcept
	{
		return *std::launder(reinterpret_cast<const T *>(m_storage));
	}

	[[clang::always_inline]] T *operator->() noexcept { return std::launder(reinterpret_cast<T *>(m_storage)); }

	[[clang::always_inline]] const T *operator->() const noexcept
	{
		return std::launder(reinterpret_cast<const T *>(m_storage));
	}

private:
	alignas(A) std::byte m_storage[S];
};

template<typename T, size_t S, size_t A>
template<typename... Args>
	requires(sizeof...(Args) != 1 || (!std::is_same_v<std::remove_cvref_t<Args>, pimpl<T, S, A>> && ...))
pimpl<T, S, A>::pimpl(Args &&...args)
{
	static_assert(S >= sizeof(T), "Storage is too small");
	static_assert(A >= alignof(T), "Alignment is too weak");
	new (m_storage) T(std::forward<Args>(args)...);
}

template<typename T, size_t S, size_t A>
pimpl<T, S, A>::pimpl(pimpl &&other) noexcept
{
	static_assert(S >= sizeof(T), "Storage is too small");
	static_assert(A >= alignof(T), "Alignment is too weak");
	static_assert(std::is_nothrow_move_constructible_v<T>, "Type must be nothrow move-constructible");
	new (m_storage) T(std::move(other.object()));
}

template<typename T, size_t S, size_t A>
pimpl<T, S, A>::pimpl(const pimpl &other)
{
	static_assert(S >= sizeof(T), "Storage is too small");
	static_assert(A >= alignof(T), "Alignment is too weak");
	static_assert(std::is_copy_constructible_v<T>, "Type must be copy-constructible");
	new (m_storage) T(other.object());
}

template<typename T, size_t S, size_t A>
pimpl<T, S, A> &pimpl<T, S, A>::operator=(pimpl &&other) noexcept
{
	static_assert(std::is_nothrow_move_assignable_v<T>, "Type must be nothrow move-assignable");
	object() = std::move(other.object());
	return *this;
}

template<typename T, size_t S, size_t A>
pimpl<T, S, A> &pimpl<T, S, A>::operator=(const pimpl &other)
{
	static_assert(std::is_copy_assignable_v<T>, "Type must be copy-assignable");
	object() = other.object();
	return *this;
}

template<typename T, size_t S, size_t A>
pimpl<T, S, A>::~pimpl() noexcept
{
	static_assert(std::is_nothrow_destructible_v<T>, "Type must be nothrow destructible");
	object().~T();
}

} // namespace extras
