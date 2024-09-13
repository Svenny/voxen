#pragma once

#include <utility>

namespace voxen
{

// Specifies behavior of a versinoning container
enum class V8gStoragePolicy {
	// Immutable containers are created as copies of mutable ones
	// and can't be modified after creation; but they can share objects
	// with previous copies where their versions did not change.
	// To achieve that, mutable-to-immutable copy constructors will take
	// two arguments - mutable container reference and an optional pointer
	// to the previous immutable one.
	//
	// An immutable container is also copyable from another immutable instance,
	// sharing ownership of all their value objects. This operation should
	// usually be pretty cheap but still not completely free.
	//
	// Selecting this storage policy will usually significantly alter
	// the container implementation and the set of available functions.
	// No modifying (non-const) methods are available for it.
	Immutable,
	// This is the default storage policy of a mutable container.
	// When creating an immutable one out of it, its value objects
	// will be be copied, if needed. Therefore mutable-to-immutable copy
	// is a non-damaging operation and is safe to do several times.
	//
	// Unlike immutable ones, mutable containers are not copy-constructible
	// from each other. Their value objects ownership is private (unique).
	Copyable,
	// This storage policy is similar to `Copyable`, but when creating an immutable
	// container, mutable value objects will be passed as non-const references
	// to immutable value constructors, meaning these can possibly modify ("damage")
	// the mutable ones. For example, an immutable value constructor might take ownership
	// of some
	//
	// Creating a valid immutable container again might be impossible (specifically
	// it might not have the "complete" value objects) if you don't pass in the
	// previous copy as well. Be very careful when using this policy.
	// It is strognly recommended to perform copies in a single place, in order,
	// and always pass in the previous copy (retaining it between the copies).
	DmgCopyable,
	// This storage policy of a mutable container shared value objects storage
	// with its immutable copies. Thus there is no value copying at all, but altering
	// the already inserted value is impossible. This policy is very similar to `Immutable`
	// but allows inserting/erasing items - i.e. modifying the container itself.
	//
	// Choose this policy if any modifications to the value objects require regenerating
	// them completely, e.g. if they hold compressed (in a broad sense) data.
	Shared,
};

// Tuple-like element of map-type container with convenient access methods.
// Supports `std::get` and structured binding syntax ("tuple protocol").
//
// On user side it will usually only be accessed through an iterator.
// Concepts are not used here as you're not supposed to manually instantiate it.
//
// `K` is key type, `VP` is value pointer (shared/unique etc.)
template<typename K, typename VP>
class V8gMapItem : public std::tuple<uint64_t, K, VP> {
public:
	V8gMapItem(uint64_t version, K key, VP value_ptr) : std::tuple<uint64_t, K, VP>(version, key, std::move(value_ptr))
	{}

	V8gMapItem(V8gMapItem &&) = default;
	V8gMapItem(const V8gMapItem &) = default;
	V8gMapItem &operator=(V8gMapItem &&) = default;
	V8gMapItem &operator=(const V8gMapItem &) = default;
	~V8gMapItem() = default;

	// Whether the value object exists. This can be false only if
	// a null pointer was explicitly inserted into the container.
	// Then this item merely indicates key presence.
	bool hasValue() const noexcept { return std::get<2>(*this) != nullptr; }

	uint64_t version() const noexcept { return std::get<0>(*this); }
	const K &key() const noexcept { return std::get<1>(*this); }
	// Value reference; undefined if `hasValue() == false`
	const auto &value() const noexcept { return *std::get<2>(*this); }
	// Value address (pointer); null if `hasValue() == false`
	const auto *valueAddr() const noexcept { return std::get<2>(*this).get(); }
	// Smart pointer to the value; mostly an implementation detail,
	// prefer to call `value()` or `valueAddr()` instead
	const VP &valuePtr() const noexcept { return std::get<2>(*this); }

	uint64_t &version() noexcept { return std::get<0>(*this); }
	K &key() noexcept { return std::get<1>(*this); }
	auto &value() noexcept { return *std::get<2>(*this); }
	auto *valueAddr() noexcept { return std::get<2>(*this).get(); }
	VP &valuePtr() noexcept { return std::get<2>(*this); }
};

} // namespace voxen

namespace std
{

template<typename K, typename VP>
struct tuple_size<voxen::V8gMapItem<K, VP>> : std::integral_constant<size_t, 3> {};

template<size_t I, typename K, typename VP>
struct tuple_element<I, voxen::V8gMapItem<K, VP>> : tuple_element<I, tuple<uint64_t, K, VP>> {};

} // namespace std
