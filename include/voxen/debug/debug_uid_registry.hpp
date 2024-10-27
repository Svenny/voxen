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

// Controls how `lookup()` will format its output
enum Format {
	// `<string> (<uid>)` if found, `<uid>` otherwise.
	// This is the default mode as it's the most informative.
	FORMAT_STRING_AND_UID = 0,
	// `<string>` if found, `<uid>` otherwise
	FORMAT_STRING_OR_UID = 1,
	// `<string>` if found, empty string otherwise
	FORMAT_STRING_ONLY = 2,
};

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
// See `Format` for the description of available format modes.
//
// Takes output by reference to reduce reallocations in bulk queries.
VOXEN_API void lookup(UID id, std::string &out, Format format = FORMAT_STRING_AND_UID);

// Simpler form of `lookup(id, out, format)`
inline std::string lookup(UID id, Format format = FORMAT_STRING_AND_UID)
{
	std::string str;
	lookup(id, str, format);
	return str;
}

} // namespace UidRegistry

} // namespace voxen::debug
