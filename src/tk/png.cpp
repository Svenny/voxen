#include <tk/png.hpp>

#define ZLIB_CONST
#include <zlib.h>

#include <bit>
#include <cassert>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

namespace tk::png
{

namespace
{

constexpr uint8_t PNG_HEADER[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
constexpr uint8_t PNG_IDAT_TYPE[4] = { 0x49, 0x44, 0x41, 0x54 }; // Letters IDAT

constexpr char UNSUPPORTED_NOTE[] = " (this implementation supports only a small subset of PNG)";

#pragma pack(push, 1)
struct IhdrChunk {
	constexpr static uint8_t CHANNELS_GREY = 0;
	constexpr static uint8_t CHANNELS_RGB = 2;
	constexpr static uint8_t CHANNELS_GREY_ALPHA = 4;
	constexpr static uint8_t CHANNELS_RGBA = 6;

	uint8_t chunk_length[4] = { 0x00, 0x00, 0x00, 0x0D }; // Always 13 bytes
	uint8_t chunk_type[4] = { 0x49, 0x48, 0x44, 0x52 };   // Letters IHDR

	uint8_t width[4];
	uint8_t height[4];
	uint8_t bit_depth;
	uint8_t colour_type;
	uint8_t compression_method = 0;
	uint8_t filter_method = 0;
	uint8_t interlace_method = 0;

	uint8_t chunk_crc[4];
};

struct IendChunk {
	uint8_t chunk_length[4] = {};                       // No data
	uint8_t chunk_type[4] = { 0x49, 0x45, 0x4E, 0x44 }; // Letters IEND
	uint8_t chunk_crc[4] = { 0xAE, 0x42, 0x60, 0x82 };  // Precomputed CRC
};
#pragma pack(pop)

void bigEndianPack(uint32_t input, void *output) noexcept
{
	auto *bytes = reinterpret_cast<uint8_t *>(output);
	bytes[0] = static_cast<uint8_t>(input >> 24U);
	bytes[1] = static_cast<uint8_t>(input >> 16U);
	bytes[2] = static_cast<uint8_t>(input >> 8U);
	bytes[3] = static_cast<uint8_t>(input);
}

uint32_t bigEndianUnpack(const void *input) noexcept
{
	auto *bytes = reinterpret_cast<const uint8_t *>(input);
	uint32_t output = uint32_t(bytes[0]) << 24U;
	output |= uint32_t(bytes[1]) << 16U;
	output |= uint32_t(bytes[2]) << 8U;
	return output | bytes[3];
}

uint32_t checksumCrc32(std::span<const std::byte> data) noexcept
{
	// Size argument to crc32 is `uInt`, not `uLong` or `size_t`.
	// So loop several times if requested size is bigger.
	constexpr size_t LIMIT = std::numeric_limits<uInt>::max();

	uLong crc = crc32(0, Z_NULL, 0);

	while (data.size() >= LIMIT) {
		crc = crc32(crc, reinterpret_cast<const Bytef *>(data.data()), LIMIT);
		data = data.subspan(LIMIT);
	}

	// Now we can safely shorten span size to `uInt`
	crc = crc32(crc, reinterpret_cast<const Bytef *>(data.data()), static_cast<uInt>(data.size()));

	// Don't know why zlib uses `uLong` for crc32 value
	return static_cast<uint32_t>(crc);
}

std::string zlibMessage(const z_stream &strm)
{
	return strm.msg ? std::string(strm.msg) : std::string("no message");
}

// Calls `deflateEnd` or `inflateEnd` when leaving the scope
template<int (*End)(z_streamp)>
struct ZStreamGuard {
	z_stream &strm;

	~ZStreamGuard() { End(&strm); }
};

size_t rowBytes(const Info &info) noexcept
{
	return size_t(info.width) * info.channels * (info.is_16bpc ? 2 : 1);
}

std::expected<std::vector<std::byte>, std::string> zlibPack(std::span<const std::byte> input, const Info &info,
	bool flip_y, int level)
{
	assert(level >= 0 && level <= 9);

	z_stream strm = {};
	if (deflateInit(&strm, level) != Z_OK) [[unlikely]] {
		return std::unexpected("zlib deflateInit failed: " + zlibMessage(strm));
	}

	ZStreamGuard<deflateEnd> guard { strm };

	std::vector<std::byte> output;
	std::byte chunk[1024];

	auto do_deflate = [&](int flush) {
		// Output buffer can be not enough for a single `deflate` call (in which case
		// `strm.avail_out` will be 0) so we might need to repeat it multiple times
		do {
			strm.avail_out = std::size(chunk);
			strm.next_out = reinterpret_cast<Byte *>(chunk);

			[[maybe_unused]] int res = deflate(&strm, flush);
			// Breaking deflate stream guarantees a bug in our code
			assert(res != Z_STREAM_ERROR);

			// How much bytes was output by `deflate` is full buffer size minus still available part
			output.insert(output.end(), chunk, chunk + (std::size(chunk) - strm.avail_out));
		} while (strm.avail_out == 0);
	};

	const int64_t height = info.height;
	const size_t row_bytes = rowBytes(info);

	// This buffer will hold the entire image row plus filter type byte.
	// Each PNG line starts with special byte specifying which filter was applied.
	// In our case this is trivially zero - we don't attempt any filtering.
	auto input_buffer = std::make_unique<Byte[]>(1 + row_bytes);
	input_buffer[0] = 0;

	// If `flip_y == true` then loop from `height - 1` to `-1` with step `-1` (excluding `-1`)
	// If `flip_y == false` then loop from `0` to `height` with step `+1` (excluding `height`)
	const int64_t first_y = flip_y ? height - 1 : 0;
	const int64_t last_y = flip_y ? -1 : height;
	const int64_t y_step = flip_y ? -1 : +1;

	for (int64_t y = first_y; y != last_y; y += y_step) {
		const std::byte *input_row = input.data() + size_t(y) * row_bytes;

		// Copy input row into the buffer
		if (std::endian::native != std::endian::big && info.is_16bpc) {
			// 16 bits per channel in little endian - byteswap them
			for (size_t i = 0; i < row_bytes; i += 2) {
				input_buffer[i + 1] = static_cast<Byte>(input_row[i + 1]);
				input_buffer[i + 2] = static_cast<Byte>(input_row[i]);
			}
		} else {
			// Trivially copy the row to input buffer
			memcpy(input_buffer.get() + 1, input_row, row_bytes);
		}

		// Feed the row into zlib
		strm.avail_in = static_cast<uInt>(1 + row_bytes);
		strm.next_in = input_buffer.get();
		// Ask zlib to finish the stream when we are on the last row
		do_deflate(y + y_step == last_y ? Z_FINISH : Z_NO_FLUSH);
		// The whole row must be consumed by zlib at this point
		assert(strm.avail_in == 0);
	}

	return output;
}

// Perform deflate when input (packed) is known to contain the whole image
// and output (unpacked) data size can be known in advance (hence "fixed")
std::expected<std::vector<std::byte>, std::string> zlibUnpackFixed(std::span<const std::byte> packed, const Info &info,
	bool flip_y)
{
	assert(packed.size() <= UINT32_MAX);

	z_stream strm = {};
	strm.avail_in = static_cast<uInt>(packed.size());
	strm.next_in = reinterpret_cast<const Bytef *>(packed.data());

	if (inflateInit(&strm) != Z_OK) [[unlikely]] {
		return std::unexpected("zlib inflateInit failed: " + zlibMessage(strm));
	}

	ZStreamGuard<inflateEnd> guard { strm };

	std::vector<std::byte> output(numRawBytes(info));

	// If `flip_y == true` then loop from `height - 1` to `-1` with step `-1` (excluding `-1`)
	// If `flip_y == false` then loop from `0` to `height` with step `+1` (excluding `height`)
	const int64_t height = info.height;
	const int64_t first_y = flip_y ? height - 1 : 0;
	const int64_t last_y = flip_y ? -1 : height;
	const int64_t y_step = flip_y ? -1 : +1;

	const size_t row_bytes = rowBytes(info);
	// Row stores one extra byte specifying which filtering was applied.
	auto row_chunk = std::make_unique<Byte[]>(row_bytes + 1);

	for (int64_t y = first_y; y != last_y; y += y_step) {
		strm.avail_out = static_cast<uInt>(row_bytes + 1);
		strm.next_out = row_chunk.get();

		int res = inflate(&strm, Z_NO_FLUSH);
		if (res != Z_OK && res != Z_STREAM_END) [[unlikely]] {
			return std::unexpected("zlib inflate failed (code " + std::to_string(res) + "): " + zlibMessage(strm));
		}

		if (strm.avail_out != 0) [[unlikely]] {
			return std::unexpected("compressed data ends before the last image row, it mismatches IHDR");
		}

		if (row_chunk[0] != 0) [[unlikely]] {
			return std::unexpected(
				"unsupported PNG line filter " + std::to_string(row_chunk[0]) + ", only 0 is supported" + UNSUPPORTED_NOTE);
		}

		std::byte *output_row = output.data() + size_t(y) * row_bytes;

		if (std::endian::native != std::endian::big && info.is_16bpc) {
			// 16 bits per channel in little endian - byteswap them.
			// One more 1 is added to offset for that first filter mode byte.
			for (size_t i = 0; i < row_bytes; i += 2) {
				output_row[i] = static_cast<std::byte>(row_chunk[i + 2]);
				output_row[i + 1] = static_cast<std::byte>(row_chunk[i + 1]);
			}
		} else {
			// Trivially copy the row to output buffer
			memcpy(output_row, row_chunk.get() + 1, row_bytes);
		}
	}

	if (strm.avail_in != 0) [[unlikely]] {
		return std::unexpected("compressed data continues after the last image row, it mismatches IHDR");
	}

	return output;
}

} // anonymous namespace

size_t numRawBytes(const Info &info) noexcept
{
	assert(info.width > 0 && info.height > 0);
	assert(info.channels > 0 && info.channels <= 4);

	return rowBytes(info) * info.height;
}

std::expected<std::vector<std::byte>, std::string> pack(std::span<const std::byte> bytes, const Info &info, bool flip_y)
{
	constexpr int COMP_LEVEL = 6;
	assert(bytes.size() == numRawBytes(info));

	auto packed = zlibPack(bytes, info, flip_y, COMP_LEVEL);
	if (!packed) [[unlikely]] {
		return std::unexpected(std::move(packed.error()));
	}

	const std::vector<std::byte> &packed_image = *packed;

	// PNG chunks have 32-bit length field which limits the maximum amount of packed data
	if (packed_image.size() > UINT32_MAX) {
		return std::unexpected(
			"packed image takes " + std::to_string(packed_image.size()) + " bytes which is too large for PNG format");
	}

	// Overhead consists of PNG header, IHDR, IDAT (3x32bit) and IEND chunks
	const size_t overhead = std::size(PNG_HEADER) + sizeof(IhdrChunk) + 3 * sizeof(uint32_t) + sizeof(IendChunk);
	std::vector<std::byte> output(packed_image.size() + overhead);
	std::span<std::byte> output_bytes(output.data(), output.size());

	// Write PNG header
	memcpy(output_bytes.data(), PNG_HEADER, std::size(PNG_HEADER));
	output_bytes = output_bytes.subspan(std::size(PNG_HEADER));

	// Fill out IHDR chunk
	IhdrChunk ihdr;
	bigEndianPack(info.width, ihdr.width);
	bigEndianPack(info.height, ihdr.height);
	ihdr.bit_depth = info.is_16bpc ? 16 : 8;

	switch (info.channels) {
	case 1:
		ihdr.colour_type = IhdrChunk::CHANNELS_GREY;
		break;
	case 2:
		ihdr.colour_type = IhdrChunk::CHANNELS_GREY_ALPHA;
		break;
	case 3:
		ihdr.colour_type = IhdrChunk::CHANNELS_RGB;
		break;
	case 4:
		ihdr.colour_type = IhdrChunk::CHANNELS_RGBA;
		break;
	}

	// CRC calculation starts from type field and includes everything up to crc field
	auto ihdr_crc_zone = std::as_bytes(std::span(ihdr.chunk_type, ihdr.chunk_crc));
	bigEndianPack(checksumCrc32(ihdr_crc_zone), ihdr.chunk_crc);

	// Write IHDR chunk
	memcpy(output_bytes.data(), &ihdr, sizeof(ihdr));
	output_bytes = output_bytes.subspan(sizeof(ihdr));

	// Write IDAT chunk length (checked above to fit in 32-bit)
	bigEndianPack(static_cast<uint32_t>(packed_image.size()), output_bytes.data());
	output_bytes = output_bytes.subspan(sizeof(uint32_t));

	// Write IDAT chunk type and packed image contents
	memcpy(output_bytes.data(), PNG_IDAT_TYPE, std::size(PNG_IDAT_TYPE));
	memcpy(output_bytes.data() + std::size(PNG_IDAT_TYPE), packed_image.data(), packed_image.size());

	// Calculate IDAT type+data crc32
	size_t crcsz = std::size(PNG_IDAT_TYPE) + packed_image.size();
	uint32_t crc = checksumCrc32(output_bytes.first(crcsz));

	// Write IDAT crc32
	output_bytes = output_bytes.subspan(crcsz);
	bigEndianPack(crc, output_bytes.data());
	output_bytes = output_bytes.subspan(sizeof(uint32_t));

	// Write IEND chunk
	IendChunk iend;
	memcpy(output_bytes.data(), &iend, sizeof(iend));
	output_bytes = output_bytes.subspan(sizeof(iend));

	// We were allocating precisely, no unwritten bytes can remain
	assert(output_bytes.empty());
	return output;
}

std::expected<Image, std::string> unpack(std::span<const std::byte> bytes, bool flip_y)
{
	// IDAT chunk length, type and CRC
	constexpr size_t IDAT_OVERHEAD = 3 * sizeof(uint32_t);

	if (bytes.size() < std::size(PNG_HEADER) + sizeof(IhdrChunk) + IDAT_OVERHEAD + sizeof(IendChunk)) [[unlikely]] {
		return std::unexpected("too few data bytes (" + std::to_string(bytes.size()) + ") to be a valid PNG stream");
	}

	if (memcmp(PNG_HEADER, bytes.data(), std::size(PNG_HEADER)) != 0) [[unlikely]] {
		return std::unexpected("PNG magic header mismatch");
	}

	// Do some validation just to be nice.
	// This is not expected to be used with anything created not by `pack()` function from above.
	IhdrChunk ihdr;
	memcpy(&ihdr, bytes.data() + std::size(PNG_HEADER), sizeof(ihdr));

	if (memcmp(ihdr.chunk_type, "IHDR", 4) != 0) [[unlikely]] {
		return std::unexpected("the first PNG chunk is not IHDR");
	}

	if (uint32_t ihdr_length = bigEndianUnpack(ihdr.chunk_length); ihdr_length != 13) [[unlikely]] {
		return std::unexpected("unexpected IHDR chunk length " + std::to_string(ihdr_length) + " (expected 13)");
	}

	auto ihdr_crc_zone = std::as_bytes(std::span(ihdr.chunk_type, ihdr.chunk_crc));
	if (bigEndianUnpack(ihdr.chunk_crc) != checksumCrc32(ihdr_crc_zone)) [[unlikely]] {
		return std::unexpected("IHDR CRC mismatch");
	}

	if (ihdr.compression_method != 0 || ihdr.filter_method != 0 || ihdr.interlace_method != 0) [[unlikely]] {
		return std::unexpected("unsupported PNG compression/filter/interlace method ("
			+ std::to_string(ihdr.compression_method) + "/" + std::to_string(ihdr.filter_method) + "/"
			+ std::to_string(ihdr.interlace_method) + ")" + UNSUPPORTED_NOTE);
	}

	if (ihdr.bit_depth != 8 && ihdr.bit_depth != 16) [[unlikely]] {
		return std::unexpected(
			"unsupported PNG bit depth " + std::to_string(ihdr.bit_depth) + ", only 8 or 16" + UNSUPPORTED_NOTE);
	}

	Image image;
	image.info.width = bigEndianUnpack(ihdr.width);
	image.info.height = bigEndianUnpack(ihdr.height);
	image.info.is_16bpc = ihdr.bit_depth == 16;

	if (image.info.width == 0 || image.info.height == 0) [[unlikely]] {
		return std::unexpected("PNG image has zero width or height");
	}

	if (ihdr.colour_type == IhdrChunk::CHANNELS_GREY) {
		image.info.channels = 1;
	} else if (ihdr.colour_type == IhdrChunk::CHANNELS_GREY_ALPHA) {
		image.info.channels = 2;
	} else if (ihdr.colour_type == IhdrChunk::CHANNELS_RGB) {
		image.info.channels = 3;
	} else if (ihdr.colour_type == IhdrChunk::CHANNELS_RGBA) {
		image.info.channels = 4;
	} else {
		return std::unexpected("unsupported PNG colour type " + std::to_string(ihdr.colour_type) + UNSUPPORTED_NOTE);
	}

	// Assume IDAT starts immediately after IHDR (as `pack()` does).
	// Once again, this implementation is extremely barebone.
	std::span<const std::byte> idat_bytes = bytes.subspan(std::size(PNG_HEADER) + sizeof(IhdrChunk));

	const uint32_t packed_image_size = bigEndianUnpack(idat_bytes.data());

	if (memcmp(idat_bytes.data() + 4, PNG_IDAT_TYPE, std::size(PNG_IDAT_TYPE)) != 0) [[unlikely]] {
		return std::unexpected(std::string("the second PNG chunk is not IDAT") + UNSUPPORTED_NOTE);
	}

	if (idat_bytes.size() < IDAT_OVERHEAD + size_t(packed_image_size)) [[unlikely]] {
		return std::unexpected("IDAT chunk length " + std::to_string(packed_image_size) + " exceeds PNG stream size");
	}

	const uint32_t actual_crc = checksumCrc32(idat_bytes.subspan(4, 4 + packed_image_size));
	const uint32_t expected_crc = bigEndianUnpack(idat_bytes.data() + 8 + packed_image_size);

	if (actual_crc != expected_crc) [[unlikely]] {
		return std::unexpected("IDAT CRC mismatch");
	}

	// Assume the image ends immediately after that
	std::span<const std::byte> iend_bytes = idat_bytes.subspan(IDAT_OVERHEAD + packed_image_size);
	// Its default constructor fills all properly
	IendChunk iend_reference;

	if (iend_bytes.size() != sizeof(IendChunk) || memcmp(iend_bytes.data(), &iend_reference, sizeof(IendChunk)) != 0)
		[[unlikely]] {
		return std::unexpected(
			std::string("PNG stream does not end with IEND immediately after IDAT") + UNSUPPORTED_NOTE);
	}

	std::span<const std::byte> packed_image = idat_bytes.subspan(8, packed_image_size);
	auto pixels = zlibUnpackFixed(packed_image, image.info, flip_y);
	if (!pixels) [[unlikely]] {
		return std::unexpected(std::move(pixels.error()));
	}

	image.pixels = std::move(*pixels);
	return image;
}

} // namespace tk::png
