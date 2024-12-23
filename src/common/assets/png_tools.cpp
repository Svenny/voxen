#include <voxen/common/assets/png_tools.hpp>

#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/hash.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#define ZLIB_CONST
#include <zlib.h>

#include <bit>
#include <cassert>
#include <vector>

namespace voxen::assets
{

namespace
{

constexpr uint8_t PNG_HEADER[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
constexpr uint8_t PNG_IDAT_TYPE[4] = { 0x49, 0x44, 0x41, 0x54 }; // Letters IDAT

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

std::vector<std::byte> zlibPack(std::span<const std::byte> input, const PngInfo &info, bool flip_y, int level)
{
	assert(level >= 0 && level <= 9);

	z_stream strm = {};
	if (deflateInit(&strm, level) != Z_OK) [[unlikely]] {
		Log::error("deflateInit failed, msg = {}", strm.msg);
		throw Exception::fromFailedCall(VoxenErrc::ExternalLibFailure, "deflateInit");
	}

	defer {
		[[maybe_unused]] int res = deflateEnd(&strm);
		// Failing deflateEnd guarantees a bug in our code
		assert(res == Z_OK);
	};

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

	const int32_t height = info.resolution.height;
	const uint32_t row_bytes = static_cast<uint32_t>(info.resolution.width) * info.channels * (info.is_16bpc ? 2 : 1);

	// This buffer will hold the entire image row plus filter type byte.
	// Each PNG line starts with special byte specifying which filter was applied.
	// In our case this is trivially zero - we don't attempt any filtering.
	auto input_buffer = std::make_unique<Byte[]>(1 + row_bytes);
	input_buffer[0] = 0;

	// If `flip_y == true` then loop from `height - 1` to `-1` with step `-1` (excluding `-1`)
	// If `flip_y == false` then loop from `0` to `height` with step `+1` (excluding `height`)
	const int32_t first_y = flip_y ? height - 1 : 0;
	const int32_t last_y = flip_y ? -1 : height;
	const int32_t y_step = flip_y ? -1 : +1;

	for (int32_t y = first_y; y != last_y; y += y_step) {
		// Copy input row into the buffer
		if (std::endian::native != std::endian::big && info.is_16bpc) {
			// 16 bits per channel in little endian - byteswap them
			for (uint32_t i = 0; i < row_bytes; i += 2) {
				input_buffer[i + 1] = static_cast<Byte>(input[i + 1]);
				input_buffer[i + 2] = static_cast<Byte>(input[i]);
			}
		} else {
			// Trivially copy the row to input buffer
			memcpy(input_buffer.get() + 1, input.data(), row_bytes);
		}
		input = input.subspan(row_bytes);

		// Feed the row into zlib
		strm.avail_in = 1 + row_bytes;
		strm.next_in = input_buffer.get();
		// Ask zlib to finish the stream when we are on the last row
		do_deflate(y + y_step == last_y ? Z_FINISH : Z_NO_FLUSH);
		// The whole row must be consumed by zlib at this point
	}

	return output;
}

// Perform deflate when input (packed) is known to contain the whole image
// and output (unpacked) data size can be known in advance (hence "fixed")
extras::dyn_array<std::byte> zlibUnpackFixed(std::span<const std::byte> packed, const PngInfo &info, bool flip_y)
{
	assert(packed.size() <= UINT32_MAX);

	z_stream strm = {};
	strm.avail_in = static_cast<uInt>(packed.size());
	strm.next_in = reinterpret_cast<const Bytef *>(packed.data());

	if (inflateInit(&strm) != Z_OK) [[unlikely]] {
		Log::error("inflateInit failed, msg = {}", strm.msg);
		throw Exception::fromFailedCall(VoxenErrc::ExternalLibFailure, "inflateInit");
	}

	defer {
		[[maybe_unused]] int res = inflateEnd(&strm);
		// Failing inflateEnd guarantees a bug in our code
		assert(res == Z_OK);
	};

	extras::dyn_array<std::byte> output(PngTools::numRawBytes(info));

	// If `flip_y == true` then loop from `height - 1` to `-1` with step `-1` (excluding `-1`)
	// If `flip_y == false` then loop from `0` to `height` with step `+1` (excluding `height`)
	const int32_t first_y = flip_y ? info.resolution.height - 1 : 0;
	const int32_t last_y = flip_y ? -1 : info.resolution.height;
	const int32_t y_step = flip_y ? -1 : +1;

	const uint32_t row_bytes = static_cast<uint32_t>(info.resolution.width) * info.channels * (info.is_16bpc ? 2 : 1);
	// Row stores one extra byte specifying which filtering was applied.
	auto row_chunk = std::make_unique<Byte[]>(row_bytes + 1);

	for (int32_t y = first_y; y != last_y; y += y_step) {
		strm.avail_out = row_bytes + 1;
		strm.next_out = row_chunk.get();

		int res = inflate(&strm, Z_NO_FLUSH);
		if (res != Z_OK && res != Z_STREAM_END) [[unlikely]] {
			Log::error("inflate failed, msg = {}, code = {}", strm.msg, res);
			throw Exception::fromFailedCall(VoxenErrc::InvalidData, "inflate");
		}

		if (strm.avail_out != 0) [[unlikely]] {
			Log::error("inflate did not fill the whole image row, PNG data likely mismatches info");
			throw Exception::fromError(VoxenErrc::InvalidData, "packed PNG data likely mismatches info");
		}

		if (row_chunk[0] != 0) [[unlikely]] {
			Log::error("Unsupported PNG line filter {} encountered (only 0 is supported)", row_chunk[0]);
			throw Exception::fromError(VoxenErrc::OutOfResource, "TODO: unsupported PNG stream");
		}

		std::byte *output_row = output.data() + y * static_cast<ptrdiff_t>(row_bytes);

		if (std::endian::native != std::endian::big && info.is_16bpc) {
			// 16 bits per channel in little endian - byteswap them.
			// One more 1 is added to offset for that first filter mode byte.
			for (uint32_t i = 0; i < row_bytes; i += 2) {
				output_row[i] = static_cast<std::byte>(row_chunk[i + 2]);
				output_row[i + 1] = static_cast<std::byte>(row_chunk[i + 1]);
			}
		} else {
			// Trivially copy the row to output buffer
			memcpy(output_row, row_chunk.get() + 1, row_bytes);
		}
	}

	if (strm.avail_in != 0) [[unlikely]] {
		Log::error("inflate did not consume the whole packed image, PNG data likely mismatches info");
		throw Exception::fromError(VoxenErrc::InvalidData, "packed PNG data likely mismatches info");
	}

	return output;
}

} // anonymous namespace

size_t PngTools::numRawBytes(const PngInfo &info) noexcept
{
	assert(info.resolution.valid());
	assert(info.channels > 0 && info.channels <= 4);

	size_t pixels = size_t(info.resolution.width) * size_t(info.resolution.height);
	return pixels * info.channels * (info.is_16bpc ? 2 : 1);
}

extras::dyn_array<std::byte> PngTools::pack(std::span<const std::byte> bytes, const PngInfo &info, bool flip_y)
{
	constexpr int COMP_LEVEL = 6;
	assert(bytes.size() == numRawBytes(info));

	std::vector<std::byte> packed_image = zlibPack(bytes, info, flip_y, COMP_LEVEL);

	// PNG chunks have 32-bit length field which limits the maximum amount of packed data
	if (packed_image.size() > UINT32_MAX) {
		Log::error("Packed image takes {} bytes which is too large for PNG format", packed_image.size());
		throw Exception::fromError(VoxenErrc::DataTooLarge, "packed image too large");
	}

	// Overhead consists of PNG header, IHDR, IDAT (3x32bit) and IEND chunks
	const size_t overhead = std::size(PNG_HEADER) + sizeof(IhdrChunk) + 3 * sizeof(uint32_t) + sizeof(IendChunk);
	extras::dyn_array<std::byte> output(packed_image.size() + overhead);
	std::span<std::byte> output_bytes(output.data(), output.size());

	// Write PNG header
	memcpy(output_bytes.data(), PNG_HEADER, std::size(PNG_HEADER));
	output_bytes = output_bytes.subspan(std::size(PNG_HEADER));

	// Fill out IHDR chunk
	IhdrChunk ihdr;
	bigEndianPack(static_cast<uint32_t>(info.resolution.width), ihdr.width);
	bigEndianPack(static_cast<uint32_t>(info.resolution.height), ihdr.height);
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

extras::dyn_array<std::byte> PngTools::unpack(std::span<const std::byte> bytes, PngInfo &info, bool flip_y)
{
	if (bytes.size() < std::size(PNG_HEADER) + sizeof(IhdrChunk)) [[unlikely]] {
		Log::error("Too few data bytes ({}) to be a valid PNG stream", bytes.size());
		throw Exception::fromError(VoxenErrc::InvalidData, "PNG data has too few bytes");
	}

	if (memcmp(PNG_HEADER, bytes.data(), std::size(PNG_HEADER)) != 0) [[unlikely]] {
		Log::error("PNG header mismatch");
		throw Exception::fromError(VoxenErrc::InvalidData, "PNG magic header mismatch");
	}

	// Do some validation just to be nice.
	// This is not expected to be used with anything created not by `pack()` function from above.
	const IhdrChunk &ihdr = *reinterpret_cast<const IhdrChunk *>(bytes.data() + std::size(PNG_HEADER));

	if (ihdr.chunk_type[0] != 'I' || ihdr.chunk_type[1] != 'H' || ihdr.chunk_type[2] != 'D' || ihdr.chunk_type[3] != 'R')
		[[unlikely]] {
		Log::error("The first chunk is not IHDR");
		throw Exception::fromError(VoxenErrc::InvalidData, "PNG first chunk is not IHDR");
	}

	if (uint32_t ihdr_length = bigEndianUnpack(ihdr.chunk_length); ihdr_length != 13) [[unlikely]] {
		Log::error("Unexpected IHDR chunk length {} (expected 13)", ihdr_length);
		throw Exception::fromError(VoxenErrc::InvalidData, "PNG IHDR chunk length mismatch");
	}

	auto ihdr_crc_zone = std::as_bytes(std::span(ihdr.chunk_type, ihdr.chunk_crc));
	if (bigEndianUnpack(ihdr.chunk_crc) != checksumCrc32(ihdr_crc_zone)) [[unlikely]] {
		Log::error("IHDR CRC mismatch");
		throw Exception::fromError(VoxenErrc::InvalidData, "PNG IHDR CRC mismatch");
	}

	if (ihdr.compression_method != 0 || ihdr.filter_method != 0 || ihdr.interlace_method != 0) [[unlikely]] {
		Log::error("Unsupported PNG compression/filter/interlace method ({}/{}/{})", ihdr.compression_method,
			ihdr.filter_method, ihdr.interlace_method);
		Log::info("Note that this implementation is extremely barebone and is not indended for production usage");
		throw Exception::fromError(VoxenErrc::OutOfResource, "TODO: unsupported PNG stream");
	}

	if (ihdr.bit_depth != 8 && ihdr.bit_depth != 16) [[unlikely]] {
		Log::error("Unsupported PNG bit depth {} (support only 8 or 16)", ihdr.bit_depth);
		throw Exception::fromError(VoxenErrc::OutOfResource, "TODO: unsupported PNG stream");
	}

	const uint32_t width = bigEndianUnpack(ihdr.width);
	const uint32_t height = bigEndianUnpack(ihdr.height);

	info.resolution = { int32_t(width), int32_t(height) };
	info.is_16bpc = ihdr.bit_depth == 16;

	if (ihdr.colour_type == IhdrChunk::CHANNELS_GREY) {
		info.channels = 1;
	} else if (ihdr.colour_type == IhdrChunk::CHANNELS_GREY_ALPHA) {
		info.channels = 2;
	} else if (ihdr.colour_type == IhdrChunk::CHANNELS_RGB) {
		info.channels = 3;
	} else if (ihdr.colour_type == IhdrChunk::CHANNELS_RGBA) {
		info.channels = 4;
	} else {
		Log::error("Unsupported PNG colour type {}", ihdr.colour_type);
		return {};
	}

	// Assume IDAT starts immediately after IHDR (as `pack()` does).
	// Once again, this implementation is extremely barebone.
	std::span<const std::byte> idat_bytes = bytes.subspan(std::size(PNG_HEADER) + sizeof(IhdrChunk));

	const uint32_t packed_image_size = bigEndianUnpack(idat_bytes.data());

	if (memcmp(idat_bytes.data() + 4, PNG_IDAT_TYPE, std::size(PNG_IDAT_TYPE)) != 0) [[unlikely]] {
		Log::error("The second PNG chunk is not IDAT");
		Log::info("Note that this implementation is extremely barebone and is not indended for production usage");
		throw Exception::fromError(VoxenErrc::OutOfResource, "TODO: unsupported PNG stream");
	}

	const uint32_t actual_crc = checksumCrc32(idat_bytes.subspan(4, 4 + packed_image_size));
	const uint32_t expected_crc = bigEndianUnpack(idat_bytes.data() + 8 + packed_image_size);

	if (actual_crc != expected_crc) [[unlikely]] {
		Log::error("IDAT CRC mismatch");
		throw Exception::fromError(VoxenErrc::InvalidData, "PNG IDAT CRC mismatch");
	}

	// Assume the image ends immediately after that
	std::span<const std::byte> iend_bytes = idat_bytes.subspan(12 + packed_image_size);
	// Its default constructor fills all properly
	IendChunk iend_reference;

	if (iend_bytes.size() != sizeof(IendChunk) || memcmp(iend_bytes.data(), &iend_reference, sizeof(IendChunk)) != 0)
		[[unlikely]] {
		Log::error("PNG is not terminated with IEND immediately after IDAT, this is unsupported");
		Log::info("Note that this implementation is extremely barebone and is not indended for production usage");
		throw Exception::fromError(VoxenErrc::OutOfResource, "TODO: unsupported PNG stream");
	}

	std::span<const std::byte> packed_image = idat_bytes.subspan(8, packed_image_size);
	return zlibUnpackFixed(packed_image, info, flip_y);
}

} // namespace voxen::assets
