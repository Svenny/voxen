#pragma once

#include <concepts>
#include <type_traits>

namespace voxen
{

// All value types used in versioning containers must support
// nothrow destructor and, if move construction/assignment is
// supported, it must be nothrow as well.
// There is no compelling reason ever to make these throwing.
template<typename T>
concept CV8GBase = std::is_nothrow_destructible_v<T>
	&& (std::is_nothrow_move_constructible_v<T> || !std::is_move_constructible_v<T>)
	&& (std::is_nothrow_move_assignable_v<T> || !std::is_move_assignable_v<T>);

// Versioning data structure search key type.
//
// Must be nothrow copy constructible/assignable and support
// ==, != and < operators defining a (semantic) total order.
//
// Keys are expected to be small (up to a few QWORDs),
// so they are always stored inline and usually passed by value.
template<typename T>
concept CV8GKey = CV8GBase<T> && std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_assignable_v<T>
	&& requires(const T &a, const T &b) {
			{ a == b } noexcept -> std::convertible_to<bool>;
			{ a != b } noexcept -> std::convertible_to<bool>;
			{ a < b } noexcept -> std::convertible_to<bool>;
		};

// Versioning data structure (mutable) value type.
// Usually will be constructed either in-place or by move
// and will not be moved after that (unless stored inline).
//
// Mutable values are usually expected to be pretty large; but they
// can also be "dispatch" objects containing just a few pointers to
// the actual state containers. Hence mutable containers might
// provide two storage policies - inline and `unique_ptr`-like.
// Inline storage will require move support.
template<typename T>
concept CV8GValue = CV8GBase<T>;

// Versioning data structure const (immutable) value type.
// Will be copy-constructed from a mutable value type, possibly
// reusing the previous immutable value (see CV8GCopyableValue).
//
// After construction the value is never altered or moved,
// so nothing is required other than a nothrow destructor.
//
// Immutable values are intended to be reused across container copies.
// To achieve that, containers employ `shared_ptr`-like techniques,
// an immutable value is never stored inline regardless of its size.
template<typename T>
concept CV8GConstValue = CV8GBase<T>;

// Versioning data structure (mutable) value type T supporting copy to an immutable value type U.
// Used in mutable-to-immutable container copies.
//
// Must support at least one of these copy construction forms:
// - U(const T &mutable, const U *old_immutable) - efficient copy variant
//   potentially reusing old contents. `old_immutable` can be null poiniter.
//   Recommended to implement this if you have nested versioning containers.
// - U(const T &mutable) - full copy variant (having no reuse opportunities).
//   Will be called only if the first variant is not available.
template<typename T, typename U>
concept CV8GCopyableValue = CV8GValue<T> && CV8GConstValue<U>
	&& (std::constructible_from<U, const T &, const U *> || std::constructible_from<U, const T &>);

} // namespace voxen
