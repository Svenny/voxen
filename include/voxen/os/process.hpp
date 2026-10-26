#pragma once

#include <voxen/visibility.hpp>

#include <cstdint>

namespace voxen::os
{

namespace Process
{

VOXEN_API int32_t getProcessId() noexcept;
VOXEN_API int32_t getThreadId() noexcept;

} // namespace Process

} // namespace voxen::os
