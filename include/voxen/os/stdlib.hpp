#pragma once

#include <cstdint>
#include <cstdio>

namespace voxen::os
{

// Various wrappers that hide some shittiness of C standard library
namespace Stdlib
{

// Get file size in bytes.
// Returns negative value (likely setting errno) upon error.
int64_t fileSize(FILE *stream);

} // namespace Stdlib

} // namespace voxen::os
