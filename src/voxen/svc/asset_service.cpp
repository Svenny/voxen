#include <voxen/svc/asset_service.hpp>

namespace voxen::svc
{

AssetService::AssetService(Config cfg) : m_cfg(std::move(cfg)) {}

AssetService::~AssetService() = default;

auto AssetService::addIndexFile(const std::filesystem::path& path) -> IndexHandle
{
	(void) path;

	constexpr uintptr_t id = 15;
	return IndexHandle(reinterpret_cast<IndexHandleImpl*>(id), Deleter(*this));
}

auto AssetService::mountArchive(const std::filesystem::path& data_relative_path, int32_t priority) -> MountHandle
{
	(void) data_relative_path;
	(void) priority;

	constexpr uintptr_t id = 25;
	return MountHandle(reinterpret_cast<MountHandleImpl*>(id), Deleter(*this));
}

auto AssetService::mountLooseDirectory(const std::filesystem::path& data_relative_path, int32_t priority) -> MountHandle
{
	(void) data_relative_path;
	(void) priority;

	constexpr uintptr_t id = 35;
	return MountHandle(reinterpret_cast<MountHandleImpl*>(id), Deleter(*this));
}

void AssetService::removeIndexHandle(IndexHandleImpl* handle) noexcept
{
	uintptr_t id = reinterpret_cast<uintptr_t>(handle);
	(void) id;
}

void AssetService::unmountHandle(MountHandleImpl* handle) noexcept
{
	uintptr_t id = reinterpret_cast<uintptr_t>(handle);
	(void) id;
}

} // namespace voxen::svc
