#pragma once

#include <system_error>

namespace voxen
{

// This error code is supplemental to exception classes and is
// intended to be tested and reacted on by exception handling.
enum class VoxenErrc : int {
	// Error happened in graphics subsystem
	GfxFailure = 1,
	// Graphics subsystem does not have the required capability
	GfxCapabilityMissing = 2,
	// Requested file does not exist or is inaccessible
	FileNotFound = 3,
	// Input data is invalid/corrupt and can't be used
	InvalidData = 4,
	// A finite resource was exhausted
	OutOfResource = 5,
	// A config object has no requested option but user assumes it exists
	OptionMissing = 6,
	// Input data exceeds the processible limit
	DataTooLarge = 7,
	// Call to external library failed for library-specific reasons
	ExternalLibFailure = 8,
};

// ADL-accessible factory for `std::error_condition { VoxenErrc }`
std::error_condition make_error_condition(VoxenErrc errc) noexcept;

} // namespace voxen

namespace std
{

// Mark `VoxenErrc` as eligible for `std::error_condition`
template<>
struct is_error_condition_enum<voxen::VoxenErrc> : true_type {};

} // namespace std
