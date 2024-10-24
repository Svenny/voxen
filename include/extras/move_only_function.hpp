#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace extras
{

// Replacement of C++23 `std::move_only_function` until we get C++23 support.
//
// Basically a prettified copy-paste of GCC's libstdc++ implementation, see
// https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/include/bits/move_only_function.h
// https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/include/bits/mofunc_impl.h
//
// I am not the author of this code.
//
// This template is not defined (per C++ reference).
// Instead, specializations for different const/&/&& qualifications are
// defined in `move_only_function_impl.hpp` with quite ugly macro codegen.
template<typename... Signature>
class move_only_function;

namespace detail
{

class move_only_function_base {
protected:
	move_only_function_base() noexcept = default;

	move_only_function_base(move_only_function_base&& other) noexcept
	{
		m_manager = std::exchange(other.m_manager, empty_manager);
		m_manager(m_storage, &other.m_storage);
	}

	move_only_function_base(const move_only_function_base&) = delete;

	move_only_function_base& operator=(move_only_function_base&& other) noexcept
	{
		m_manager(m_storage, nullptr);
		m_manager = std::exchange(other.m_manager, empty_manager);
		m_manager(m_storage, &other.m_storage);
		return *this;
	}

	move_only_function_base& operator=(const move_only_function_base&) = delete;

	move_only_function_base& operator=(std::nullptr_t) noexcept
	{
		m_manager(m_storage, nullptr);
		m_manager = empty_manager;
		return *this;
	}

	~move_only_function_base() noexcept { m_manager(m_storage, nullptr); }

	void swap(move_only_function_base& other) noexcept
	{
		// Order of operations here is more efficient if `other` is empty
		storage_t tmp;
		// 1. Move storage from `other` to `tmp`
		other.m_manager(tmp, &other.m_storage);
		// 2. Move our storage to `other`
		m_manager(other.m_storage, &m_storage);
		// 3. Move `tmp` storage into us
		other.m_manager(m_storage, &tmp);
		// Exchange manager functions
		std::swap(m_manager, other.m_manager);
	}

	template<typename _Tp, typename... _Args>
	constexpr static bool is_init_nothrow() noexcept
	{
		if constexpr (is_stored_locally<_Tp>) {
			return std::is_nothrow_constructible_v<_Tp, _Args...>;
		}
		return false;
	}

	template<typename T, typename... Args>
	void init(Args&&... args) noexcept(is_init_nothrow<T, Args...>())
	{
		if constexpr (is_stored_locally<T>) {
			::new (m_storage.addr()) T(std::forward<Args>(args)...);
		} else {
			m_storage.ptr = new T(std::forward<Args>(args)...);
		}

		m_manager = &real_manager<T>;
	}

	template<typename T, typename Self>
	static T* access(Self* self) noexcept
	{
		if constexpr (is_stored_locally<std::remove_const_t<T>>) {
			return static_cast<T*>(self->m_storage.addr());
		} else {
			return static_cast<T*>(self->m_storage.ptr);
		}
	}

private:
	struct storage_t {
		// We want to have enough space to store a simple delegate type
		struct delegate_t {
			void (storage_t::*pfm)();
			storage_t* obj;
		};

		union {
			void* ptr;
			alignas(delegate_t) alignas(void (*)()) std::byte bytes[sizeof(delegate_t)];
		};

		void* addr() noexcept { return &bytes[0]; }
		const void* addr() const noexcept { return &bytes[0]; }
	};

	template<typename T>
	constexpr static bool is_stored_locally = sizeof(T) <= sizeof(storage_t) && alignof(T) <= alignof(storage_t)
		&& std::is_nothrow_move_constructible_v<T>;

	// A function that either destroys the target object stored in `target`
	// if `src` is nullptr, or moves the target object from `*src` to `target`.
	using manager_fn_t = void (*)(storage_t& target, storage_t* src) noexcept;

	static void empty_manager(storage_t&, storage_t*) noexcept {}

	template<typename T>
	static void real_manager(storage_t& target, storage_t* src) noexcept
	{
		if constexpr (is_stored_locally<T>) {
			if (src) {
				T* rval = static_cast<T*>(src->addr());
				::new (target.addr()) T(std::move(*rval));
				rval->~T();
			} else {
				static_cast<T*>(target.addr())->~T();
			}
		} else {
			if (src) {
				// `str->ptr` must not be used after this call, not resetting it to null
				target.ptr = src->ptr;
			} else {
				delete static_cast<T*>(target.ptr);
			}
		}
	}

	storage_t m_storage;
	manager_fn_t m_manager = empty_manager;
};

template<typename T>
constexpr bool is_move_only_function_v = false;

template<typename T>
constexpr bool is_move_only_function_v<move_only_function<T>> = true;

} // namespace detail

} // namespace extras

// non-const value
#include "move_only_function_impl.hpp"

// const value
#define _EXTRAS_MOFUNC_CV const
#include "move_only_function_impl.hpp"

// non-const lvalue ref
#define _EXTRAS_MOFUNC_REF &
#include "move_only_function_impl.hpp"

// non-const rvalue ref
#define _EXTRAS_MOFUNC_REF &&
#include "move_only_function_impl.hpp"

// const lvalue ref
#define _EXTRAS_MOFUNC_CV const
#define _EXTRAS_MOFUNC_REF &
#include "move_only_function_impl.hpp"

// const rvalue ref (wtf?)
#define _EXTRAS_MOFUNC_CV const
#define _EXTRAS_MOFUNC_REF &&
#include "move_only_function_impl.hpp"
