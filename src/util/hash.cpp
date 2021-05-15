#include <voxen/util/hash.hpp>

namespace voxen
{

uint64_t hashFnv1a(const void *data, std::size_t size) noexcept
{
	auto bytes = reinterpret_cast<const uint8_t *>(data);
	uint64_t result = 0xCBF29CE484222325;
	for (std::size_t i = 0; i < size; i++) {
		result *= 0x100000001B3;
		result ^= uint64_t(bytes[i]);
	}
	return result;
}

}
