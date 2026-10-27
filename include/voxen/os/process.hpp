#pragma once

#include <voxen/visibility.hpp>

#include <cstdint>

namespace voxen::os::Process
{

VOXEN_API int32_t getProcessId() noexcept;
VOXEN_API int32_t getThreadId() noexcept;

} // namespace voxen::os::Process
