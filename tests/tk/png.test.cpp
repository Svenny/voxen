#include "../test_common.hpp"

#include <cstring>
#include <span>

import tk.png;

namespace tk::png
{

namespace
{

std::vector<std::byte> makePixels(const Info &info)
{
	std::vector<std::byte> pixels(numRawBytes(info));
	for (size_t i = 0; i < pixels.size(); i++) {
		pixels[i] = std::byte((i * 37 + i / 7) & 0xFF);
	}
	return pixels;
}

std::vector<std::byte> flipRows(std::span<const std::byte> pixels, const Info &info)
{
	const size_t row_bytes = numRawBytes(info) / info.height;

	std::vector<std::byte> flipped(pixels.size());
	for (size_t y = 0; y < info.height; y++) {
		memcpy(flipped.data() + y * row_bytes, pixels.data() + (info.height - 1 - y) * row_bytes, row_bytes);
	}
	return flipped;
}

} // namespace

TEST_CASE("'png' round-trips images of all supported formats", "[tk::png]")
{
	for (uint8_t channels = 1; channels <= 4; channels++) {
		for (bool is_16bpc : { false, true }) {
			const Info info { .width = 13, .height = 7, .is_16bpc = is_16bpc, .channels = channels };
			const std::vector<std::byte> pixels = makePixels(info);

			auto packed = pack(pixels, info, false);
			REQUIRE(packed.has_value());

			auto image = unpack(*packed, false);
			REQUIRE(image.has_value());
			CHECK(image->info.width == info.width);
			CHECK(image->info.height == info.height);
			CHECK(image->info.is_16bpc == info.is_16bpc);
			CHECK(image->info.channels == info.channels);
			CHECK(image->pixels == pixels);
		}
	}
}

TEST_CASE("'png' writes standard PNG headers", "[tk::png]")
{
	const Info info { .width = 0x123, .height = 0x45, .is_16bpc = true, .channels = 4 };
	auto packed = pack(makePixels(info), info, false);
	REQUIRE(packed.has_value());
	REQUIRE(packed->size() > 33);

	const auto *bytes = reinterpret_cast<const uint8_t *>(packed->data());
	constexpr uint8_t SIGNATURE[] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
	CHECK(memcmp(bytes, SIGNATURE, sizeof(SIGNATURE)) == 0);

	// IHDR length and type, width, height, then 16 bits, RGBA, no compression/filter/interlace variants
	constexpr uint8_t IHDR[] = { 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0x01, 0x23, 0, 0, 0, 0x45, 16, 6, 0, 0, 0 };
	CHECK(memcmp(bytes + 8, IHDR, sizeof(IHDR)) == 0);
}

TEST_CASE("'png' flips images vertically", "[tk::png]")
{
	const Info info { .width = 5, .height = 4, .is_16bpc = false, .channels = 3 };
	const std::vector<std::byte> pixels = makePixels(info);
	const std::vector<std::byte> flipped = flipRows(pixels, info);

	auto packed_flipped = pack(pixels, info, true);
	auto packed_reference = pack(flipped, info, false);
	REQUIRE(packed_flipped.has_value());
	REQUIRE(packed_reference.has_value());
	CHECK(*packed_flipped == *packed_reference);

	auto image = unpack(*packed_reference, true);
	REQUIRE(image.has_value());
	CHECK(image->pixels == pixels);
}

TEST_CASE("'png' rejects malformed streams", "[tk::png]")
{
	const Info info { .width = 8, .height = 8, .is_16bpc = false, .channels = 1 };
	auto packed = pack(makePixels(info), info, false);
	REQUIRE(packed.has_value());

	SECTION("Truncated")
	{
		std::span<const std::byte> bytes = *packed;
		CHECK_FALSE(unpack(bytes.first(bytes.size() - 1), false).has_value());
		CHECK_FALSE(unpack(bytes.first(20), false).has_value());
		CHECK_FALSE(unpack({}, false).has_value());
	}

	SECTION("Bad signature")
	{
		(*packed)[1] = std::byte('X');
		CHECK_FALSE(unpack(*packed, false).has_value());
	}

	SECTION("Bad IHDR CRC")
	{
		// Corrupt width
		(*packed)[19] ^= std::byte(1);
		CHECK_FALSE(unpack(*packed, false).has_value());
	}

	SECTION("Bad IDAT CRC")
	{
		// Corrupt the first byte of compressed data
		(*packed)[41] ^= std::byte(1);
		CHECK_FALSE(unpack(*packed, false).has_value());
	}

	SECTION("Bad IDAT length")
	{
		(*packed)[33] = std::byte(0x7F);
		CHECK_FALSE(unpack(*packed, false).has_value());
	}
}

} // namespace tk::png
