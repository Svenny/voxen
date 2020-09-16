#pragma once

#include <cstdint>

namespace voxen
{

// Compute fast non-cryptographic FNV-1a hash
uint64_t hashFnv1a(const void *data, std::size_t size) noexcept;

}
