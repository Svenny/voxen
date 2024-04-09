#include <voxen/common/uid.hpp>

#include <cinttypes>
#include <cstdio>
#include <random>

namespace voxen
{

static_assert(sizeof(UID) == 16);

// It works!
static_assert(UID("c1bf2846-ff1f9f34-a0abff03-e68abb9b") == UID(0xc1bf2846ff1f9f34U, 0xa0abff03e68abb9bU));
// Full compile-time validation, neither of this will compile:
// static_assert(UID("c1bf2846-ff1f9f34-a0abff03-#68abb9b").v0 != 0);
// static_assert(UID("c1bf2846-ff1f9f34-a0abff03@e68abb9b").v0 != 0);
// static_assert(UID("c1bf2846-ff1f9f34-a0abff03-e68").v0 != 0);
// static_assert(UID("c1bf2846-ff1f9f34-a0abff03-e68abb9b###").v0 != 0);
// static_assert(UID("c1bf2846-ff1f9f34-a0abff03-e68ABB9b").v0 != 0);

void UID::toChars(std::span<char, CHAR_REPR_LENGTH> out) const noexcept
{
	constexpr char format[] = "%08" PRIx32 "-%08" PRIx32 "-%08" PRIx32 "-%08" PRIx32;

	uint32_t split[4];
	split[0] = static_cast<uint32_t>(v0 >> 32u);
	split[1] = static_cast<uint32_t>(v0);
	split[2] = static_cast<uint32_t>(v1 >> 32u);
	split[3] = static_cast<uint32_t>(v1);

	snprintf(out.data(), out.size(), format, split[0], split[1], split[2], split[3]);
}

UID UID::generateRandom()
{
	using rt = std::random_device::result_type;
	static_assert(sizeof(UID) % sizeof(rt) == 0);

	std::random_device rng;

	rt values[sizeof(UID) / sizeof(rt)];
	for (rt &v : values) {
		v = rng();
	}

	// Idiomatic type punning
	UID uid;
	memcpy(&uid, values, sizeof(UID));
	return uid;
}

} // namespace voxen
