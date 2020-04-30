#pragma once

#include <voxen/config.hpp>

#include <experimental/source_location>

namespace voxen
{

[[noreturn]] void vxAssertFail(std::experimental::source_location where) noexcept;

inline void vxAssert(bool expr, std::experimental::source_location where =
      std::experimental::source_location::current()) noexcept {
	if constexpr (BuildConfig::kIsDebugBuild) {
		if (!expr)
			vxAssertFail(where);
	}
}

inline void vxAssertStrong(bool expr, std::experimental::source_location where =
      std::experimental::source_location::current()) noexcept {
	if (!expr)
		vxAssertFail(where);
}

}
