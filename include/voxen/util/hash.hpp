#pragma once

#include <cstddef>
#include <cstdint>

namespace voxen
{

// Compute fast non-cryptographic FNV-1a hash
uint64_t hashFnv1a(const void *data, size_t size) noexcept;

// Compute a very fast non-cryptographic hash based on xorshift64 RNG
uint64_t hashXorshift32(const uint32_t *data, size_t count) noexcept;
// Compute a very fast non-cryptograhpic hash based on xorshift64 RNG
uint64_t hashXorshift64(const uint64_t *data, size_t count) noexcept;

}
