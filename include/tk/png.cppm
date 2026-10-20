module;

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

export module tk.png;

// Minimal PNG encoder and decoder, for images without palettes or interlacing.
//
// NOTE: this is extremely bare-bone and is not suitable for general use.
// The decoder rejects most of valid PNG streams not similar to those produced by `pack()`.

export namespace tk::png
{

struct Info {
	uint32_t width = 0;
	uint32_t height = 0;
	// Whether channels are 8-bit or 16-bit.
	// 16-bit channels are laid out in memory as native-endian `uint16_t` values.
	bool is_16bpc = false;
	// 1 - grayscale
	// 2 - grayscale+alpha
	// 3 - RGB
	// 4 - RGB+alpha
	uint8_t channels = 0;
};

// Raw (uncompressed) image, rows are tightly packed
struct Image {
	Info info;
	std::vector<std::byte> pixels;
};

// Number of bytes needed for raw (uncompressed) data of image defined by `info`.
// NOTE: `info` must define a valid image (positive resolution and valid channels count).
size_t numRawBytes(const Info &info) noexcept;

// Pack raw image to PNG format, returning array of packed bytes (full PNG stream with headers etc).
// The first row in memory is assumed topmost, use `flip_y` to change that (flip image vertically).
// Returns the error message on failure.
//
// NOTE: `info` must define a valid image (positive resolution and valid channels count).
// NOTE: `bytes.size()` must be equal to `numRawBytes(info)`.
std::expected<std::vector<std::byte>, std::string> pack(std::span<const std::byte> bytes, const Info &info,
	bool flip_y);

// Unpack PNG stream into raw image. Returns the error message if the data cannot be decoded.
// The first row in memory is assumed topmost, use `flip_y` to change that (flip image vertically).
std::expected<Image, std::string> unpack(std::span<const std::byte> bytes, bool flip_y);

} // namespace tk::png
