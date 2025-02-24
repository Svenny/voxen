#pragma once

namespace voxen
{

class ScratchMemoryAllocator;
class ScratchMemoryAllocatorScope;

template<typename T>
struct TScratchMemoryAllocator;

namespace detail
{

class ScratchMemoryAllocatorImpl;

}

} // namespace voxen
