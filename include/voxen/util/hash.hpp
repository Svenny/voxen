#pragma once

#include <voxen/common/uid.hpp>
#include <voxen/visibility.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace voxen
{

// Collection of hash/checksum utilities
namespace Hash
{

// Fixed-size (64-bit input) XXH64 with zero seed, can be useful to make
// well-distributed bits out of anything. XXH64 is bijective for 64-bit inputs
// so you can even directly compare hashes instead of keys <= 8 bytes.
VOXEN_API uint64_t xxh64Fixed(uint64_t data) noexcept;

// "Extend" a small-dimension (64 bits or smaller) key with whatever meaning to UID.
// `pivot_uid` should be a (fixed) random UID, it defines a subsystem (domain) where
// this key belongs to. The idea is that if the same key is used in different domains
// it will be converted to different UIDs using this pivot, eliminating UID collisions.
VOXEN_API UID keyToUid(UID pivot_uid, uint64_t key) noexcept;

} // namespace Hash

// Compute fast non-cryptographic FNV-1a hash
uint64_t hashFnv1a(const void *data, size_t size) noexcept;

// Compute a very fast non-cryptographic hash based on xorshift64 RNG
uint64_t hashXorshift32(const uint32_t *data, size_t count) noexcept;
// Compute a very fast non-cryptograhpic hash based on xorshift64 RNG
uint64_t hashXorshift64(const uint64_t *data, size_t count) noexcept;

// Compute fast non-cryptographic CRC32 checksum
uint32_t checksumCrc32(std::span<const std::byte> data) noexcept;

} // namespace voxen
