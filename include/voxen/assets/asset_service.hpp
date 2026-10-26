#pragma once

// DRAFT: AssetService skeleton, nothing uses it yet. Most methods are declared, not defined

#include <voxen/assets/asset_base.hpp>
#include <voxen/svc/pipe_memory_function.hpp>
#include <voxen/svc/service_base.hpp>
#include <voxen/svc/task_coro.hpp>
#include <voxen/visibility.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>

namespace voxen::assets
{

using AssetLoadResult = std::expected<AssetBasePtr, std::error_condition>;

class VOXEN_API AssetService final : public svc::IService {
private:
	struct IndexHandleImpl;
	struct MountHandleImpl;

	struct Deleter {
		void operator()(IndexHandleImpl *handle) noexcept { me.removeIndexHandle(handle); }
		void operator()(MountHandleImpl *handle) noexcept { me.unmountHandle(handle); }

		AssetService &me;
	};

	struct AssetLoadAwaitable {
		constexpr bool await_ready() const noexcept { return false; }
	};

public:
	constexpr static UID SERVICE_UID = UID("c97e41ac-5bc462ce-c01451ba-1943917c");

	struct Config {
		std::filesystem::path hot_load_directory_path;
	};

	using IndexHandle = std::unique_ptr<IndexHandleImpl, Deleter>;
	using MountHandle = std::unique_ptr<MountHandleImpl, Deleter>;

	AssetService(Config cfg);
	AssetService(AssetService &&) = delete;
	AssetService(const AssetService &) = delete;
	AssetService &operator=(AssetService &&) = delete;
	AssetService &operator=(const AssetService &) = delete;
	~AssetService() override;

	UID serviceUid() const noexcept override { return SERVICE_UID; }

	void registerAssetType();

	IndexHandle addIndexFile(const std::filesystem::path &path);

	MountHandle mountArchive(const std::filesystem::path &data_relative_path, int32_t priority);
	MountHandle mountLooseDirectory(const std::filesystem::path &data_relative_path, int32_t priority);

	void queryAsset(UID uid);

	void asyncLoadAsset(UID uid, svc::PipeMemoryFunction<void()> completion_fn);
	AssetLoadAwaitable coroLoadAsset(UID uid);

private:
	Config m_cfg;

	void removeIndexHandle(IndexHandleImpl *handle) noexcept;
	void unmountHandle(MountHandleImpl *handle) noexcept;
};

} // namespace voxen::assets
