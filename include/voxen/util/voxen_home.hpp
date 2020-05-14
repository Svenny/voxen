#pragma once

#include <cassert>
#include <filesystem>
#include <voxen/config.hpp>

using namespace std::filesystem;

namespace voxen {

static path voxenHome() {
	if (BuildConfig::kIsDeployBuild) {
		assert(false);
		return path(); // TODO
	} else {
		return current_path();
	}
}

}
