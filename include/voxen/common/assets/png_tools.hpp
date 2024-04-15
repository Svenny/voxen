#pragma once

#include <voxen/util/resolution.hpp>
#include <voxen/visibility.hpp>

#include <extras/dyn_array.hpp>

#include <cstddef>
#include <span>

namespace voxen::assets
{

struct PngInfo {
	Resolution resolution;
	// Whether channels are 8-bit or 16-bit.
	// 16-bit channels are laid out in memory as native-endian `uint16_t` values.
	bool is_16bpc = false;
	// 1 - grayscale
	// 2 - grayscale+alpha
	// 3 - RGB
	// 4 - RGB+alpha
	uint8_t channels = 0;
};

namespace PngTools
{

// Number of bytes needed for raw (uncompressed) data of image defined by `info`.
// NOTE: `info` must define a valid image (positive resolution and valid channels count).
VOXEN_API size_t numRawBytes(const PngInfo &info) noexcept;

// Pack raw image to PNG format, returning array of packed bytes (full PNG stream with headers etc).
// The first row in memory is assumed topmost, use `flip_y` to change that (flip image vertically).
// NOTE: `info` must define a valid image (positive resolution and valid channels count).
// NOTE: `bytes.size()` must be equal to `numRawBytes(info)`.
VOXEN_API extras::dyn_array<std::byte> pack(std::span<const std::byte> bytes, const PngInfo &info, bool flip_y);

} // namespace PngTools

} // namespace voxen::assets
