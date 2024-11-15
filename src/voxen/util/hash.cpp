#include <voxen/util/hash.hpp>

#define ZLIB_CONST
#include <zlib.h>

#include <bit>
#include <limits>

namespace voxen
{

namespace Hash
{

uint64_t xxh64Fixed(uint64_t data) noexcept
{
	constexpr uint64_t PRIME1 = 11400714785074694791ULL;
	constexpr uint64_t PRIME2 = 14029467366897019727ULL;
	constexpr uint64_t PRIME3 = 1609587929392839161ULL;
	constexpr uint64_t PRIME4 = 9650029242287828579ULL;
	// `prime5` + fixed 8-byte size
	constexpr uint64_t INIT = 2870177450012600261ULL + 8ULL;

	// Perform one single 64-bit processing step
	data = std::rotl(data * PRIME2, 31) * PRIME1;
	uint64_t result = std::rotl(INIT ^ data, 27) * PRIME1 + PRIME4;
	result ^= result >> 33;
	result *= PRIME2;
	result ^= result >> 29;
	result *= PRIME3;
	result ^= result >> 32;
	// Note that all operations are easily reversible
	return result;
}

UID keyToUid(UID pivot_uid, uint64_t key) noexcept
{
	pivot_uid.v1 ^= xxh64Fixed(pivot_uid.v0 ^ key);
	return pivot_uid;
}

} // namespace Hash

uint64_t hashFnv1a(const void *data, size_t size) noexcept
{
	auto bytes = reinterpret_cast<const uint8_t *>(data);
	uint64_t result = 0xCBF29CE484222325;
	for (size_t i = 0; i < size; i++) {
		result *= 0x100000001B3;
		result ^= uint64_t(bytes[i]);
	}
	return result;
}

constexpr static uint64_t XORSHIFT64_SEED = 0x2545F4914F6CDD1DULL;

static uint64_t feedXorshift64(uint64_t prev, uint64_t next) noexcept
{
	uint64_t x = prev + next;
	x ^= x << 13u;
	x ^= x >> 7u;
	x ^= x << 17u;
	return x;
}

uint64_t hashXorshift32(const uint32_t *data, size_t count) noexcept
{
	uint64_t result = XORSHIFT64_SEED;
	for (size_t i = 0; i < count; i++) {
		result = feedXorshift64(result, data[i]);
	}
	return result;
}

uint64_t hashXorshift64(const uint64_t *data, size_t count) noexcept
{
	uint64_t result = XORSHIFT64_SEED;
	for (size_t i = 0; i < count; i++) {
		result = feedXorshift64(result, data[i]);
	}
	return result;
}

uint32_t checksumCrc32(std::span<const std::byte> data) noexcept
{
	// Size argument to crc32 is `uInt`, not `uLong` or `size_t`.
	// So loop several times if requested size is bigger.
	constexpr size_t limit = std::numeric_limits<uInt>::max();

	uLong crc = crc32(0, Z_NULL, 0);

	while (data.size() >= limit) {
		crc = crc32(crc, reinterpret_cast<const Bytef *>(data.data()), limit);
		data = data.subspan(limit);
	}

	// Now we can safely shorten span size to `uInt`
	crc = crc32(crc, reinterpret_cast<const Bytef *>(data.data()), static_cast<uInt>(data.size()));

	// Don't know why zlib uses `uLong` for crc32 value
	return static_cast<uint32_t>(crc);
}

} // namespace voxen
