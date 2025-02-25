#pragma once

#include <voxen/common/common_fwd.hpp>

#include <list>
#include <memory>
#include <vector>

namespace voxen
{

template<typename T>
using scratch_list = std::list<T, TScratchMemoryAllocator<T>>;

template<typename T>
using scratch_vector = std::vector<T, TScratchMemoryAllocator<T>>;

struct ScratchMemoryDeleter {
	template<typename T>
	void operator()(T *ptr) noexcept
	{
		ptr->~T();
	}
};

template<typename T, typename D = ScratchMemoryDeleter>
using scratch_unique_ptr = std::unique_ptr<T, D>;

} // namespace voxen
