#pragma once

#include <voxen/config.hpp>

#include <cassert>
#include <filesystem>

namespace voxen
{

inline std::filesystem::path voxenHome() {
	if constexpr (BuildConfig::kIsDeployBuild) {
		// TODO: implement me
		assert(false);
		return std::filesystem::path();
	} else {
		return std::filesystem::current_path();
	}
}

}
