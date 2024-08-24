#pragma once

#include <concepts>
#include <type_traits>

namespace voxen
{

// All value types used in versioning containers must support
// nothrow destructor and, if move construction/assignment is
// supported, it must be nothrow as well.
// There is no compelling reason ever to make these throwing.
//
// Array types are forbidden to save the remaining bits of my sanity
// and not have to deal with "array->pointer decay" bullshit here.
// Just use `std::array`, it is not considered array for this check.
template<typename T>
concept CV8gBase = std::is_nothrow_destructible_v<T> && !std::is_array_v<T>
	&& (std::is_nothrow_move_constructible_v<T> || !std::is_move_constructible_v<T>)
	&& (std::is_nothrow_move_assignable_v<T> || !std::is_move_assignable_v<T>);

// Search key type for versioning data structures.
//
// Must be nothrow copy constructible/assignable and support
// ==, != and < operators defining a (semantic) total order.
//
// Keys are expected to be small (up to a few QWORDs),
// so they are always stored inline and passed by value.
template<typename T>
concept CV8gKey = CV8gBase<T> && std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_assignable_v<T>
	&& requires(const T &a, const T &b) {
			{ a == b } noexcept -> std::convertible_to<bool>;
			{ a != b } noexcept -> std::convertible_to<bool>;
			{ a < b } noexcept -> std::convertible_to<bool>;
		};

// Value type for versioning data structures.
//
// Value objects are expected to be pretty large to justify the need
// to use versioning. Otherwise it would be cheap to copy them every time.
// But in order to support various mutable-to-immutable copy (maybe ownership-taking)
// and possibly user-defined storage policies (e.g. object pools) they need
// to be accessed through some pointer-like handles (e.g. `unique_ptr`).
//
// Therefore, containers will not store this type directly inline but
// rather wrap it in some smart pointer-like object. Hence it needs
// not be moveable/copyable, though having these operations might allow
// more efficient implementation of certain operations.
//
// If used in an immutable container, objects of this type can be shared
// between several container versions and accessed simultaneously by multiple
// threads, but these accesses will always happen via const reference.
// Ensuring thread safety of such accesses is your responsibility, but it
// should be a no-op for most sane objects (without `mutable` fields etc.)
template<typename T>
concept CV8gValue = CV8gBase<T>;

// Value type T in a mutable versioning data structure supporting copy
// to a value type U in immutable variant (snapshot) of this structure.
//
// Must support at least one of these copy construction forms:
// - U(const T &mutable, const U *old_immutable) - efficient copy variant
//   potentially reusing old contents. `old_immutable` can be null poiniter.
//   Recommended to implement this if you have nested versioning containers.
// - U(const T &mutable) - full copy variant (having no reuse opportunities).
//   Will be called only if the first variant is not available.
//
// Also see `V8gStoragePolicy::Copyable`.
template<typename T, typename U>
concept CV8gCopyableValue = CV8gValue<T> && CV8gValue<U>
	&& (std::constructible_from<U, const T &, const U *> || std::constructible_from<U, const T &>);

// Similar to `CV8gCopyableValue` with the only difference being
// non-const mutable value reference.
// This allows to perform a "damaging" copy, e.g. to "steal" parts of
// the mutable value object. Might be useful to eliminate unnecessary
// copies of data, both in time and memory, when this data needs not
// remain in the mutable object.
//
// Also see `V8gStoragePolicy::DmgCopyable`.
template<typename T, typename U>
concept CV8gDmgCopyableValue = CV8gValue<T> && CV8gValue<U>
	&& (std::constructible_from<U, T &, const U *> || std::constructible_from<U, T &>);

// Value type T in a mutable versioning data structure supporting shared storage with
// a value type U in immutable variant (snapshot) of this structure.
// These values don't have multiple copies but must be completely replaced on every update.
//
// Also see `V8gStoragePolicy::Shared`.
template<typename T, typename U>
concept CV8gSharedValue = CV8gValue<T> && CV8gValue<U> && (std::convertible_to<T *, U *>);

} // namespace voxen
