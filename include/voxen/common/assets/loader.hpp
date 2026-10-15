#pragma once

#include <voxen/common/assets/asset.hpp>

#include <atomic>

namespace voxen::assets
{

class Loader {
public:
	constexpr static uint64_t INVALID_ID = UINT64_MAX;

	struct LoadRequest {
		uint32_t first_tier = 0;
		uint32_t num_tiers = 1;
		std::atomic_bool *completion_flags = nullptr;
	};

	uint64_t findId(const char *name);
	std::optional<AssetDescriptor> findDescriptor(uint64_t id);

	void loadAsync();

private:
};

} // namespace voxen::assets
