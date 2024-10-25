#pragma once

#include <voxen/common/uid.hpp>

#include <string>
#include <string_view>

namespace voxen::debug
{

// A "database" mapping UIDs to human-readable string descriptions.
// This feature has negligible performance impact and some (minor) memory overhead.
//
// Currently there is no switch to turn the feature off to save memory.
// We should first improve the implementation if memory becomes an issue.
namespace UidRegistry
{

// Register string associated with `id`, overwriting the previous one, if any.
// String pointed to by `view` must remain allocated until either the
// next registration of `id`, call to `unregister(id)` or the program exit.
//
// WARNING: this function is unsafe if not used carefully!
// You are *strongly* advised to call it only with string literals i.e.
//     registerLiteral(my_uid, "my_module/my_domain/my_object_name/...");
// Non-literal strings can be passed here too but they must strictly adhere
// to the above lifetime requirements or `lookup` will return dangling pointers.
VOXEN_API void registerLiteral(UID id, std::string_view view);

// Register string associated with `id`, overwriting the previous one, if any.
// String pointed to by `view` is copied, there are no restrictions on its lifetime.
//
// NOTE: if you are unsure which registration function to choose, use this one.
VOXEN_API void registerString(UID id, std::string_view view);

// Remove string associated with `id` from the registry.
// Calling this function is not strictly necessary, do it if you
// do not expect to meet `id` anywhere for the foreseeable future.
VOXEN_API void unregister(UID id) noexcept;

// Find the string associated with `id` and copy it to `out`.
// Makes an empty string if no registered string is found.
//
// Takes output by reference to reduce reallocations in bulk queries.
VOXEN_API void lookup(UID id, std::string &out);

// Simpler form of `lookup(id, out)`
inline std::string lookup(UID id)
{
	std::string str;
	lookup(id, str);
	return str;
}

// Same as `lookup` but (instead of an empty string) writes `id`
// converted to string (see `UID::toChars()`) if nothing is found.
//
// Takes output by reference to reduce reallocations in bulk queries.
VOXEN_API void lookupOrPrint(UID id, std::string &out);

// Simpler form of `lookupOrPrint(id, out)`
inline std::string lookupOrPrint(UID id)
{
	std::string str;
	lookupOrPrint(id, str);
	return str;
}

} // namespace UidRegistry

} // namespace voxen::debug
