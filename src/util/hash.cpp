#include <voxen/util/hash.hpp>

namespace voxen
{

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

}
