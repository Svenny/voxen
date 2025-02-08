#pragma once

#include <extras/move_only_function.hpp>
#include <voxen/svc/service_base.hpp>
#include <voxen/visibility.hpp>
#include <voxen/world/world_fwd.hpp>

#include <extras/pimpl.hpp>

#include <filesystem>
#include <system_error>

namespace voxen::world
{

class VOXEN_API ControlService : public svc::IService {
public:
	struct StartRequest {
		// Path to the world save storage directory.
		// If it does not exist or does not contain a valid world data,
		// a new world will be generated and stored there. Otherwise the
		// world will be loaded from the directory.
		//
		// Loading will fail if this directory is not accessible (e.g. no permission)
		// or it contains world data that cannot be loaded (e.g. broken save).
		std::filesystem::path storage_directory;
		// This callback is called periodically from an unspecified thread
		// while the world is starting to update the user-visible progress meter.
		// It receives the current estimation of progress in [0..1] range.
		extras::move_only_function<void(float)> progress_callback;
		// This callback is called from an unspecified thread after world
		// starting completes. The world connection is established only if
		// the reported error condition is zero.
		extras::move_only_function<void(std::error_condition)> result_callback;
	};

	struct SaveRequest {
		// This callback is called periodically from an unspecified thread
		// while the world is saving to update the user-visible progress meter.
		// It receives the current estimation of progress in [0..1] range.
		extras::move_only_function<void(float)> progress_callback;
		// This callback is called from an unspecified thread after world
		// saving/stopping completees. If the reported error condition
		// is not zero then world saving failed.
		extras::move_only_function<void(std::error_condition)> result_callback;
	};

	constexpr static UID SERVICE_UID = UID("cdc4d6ea-aefc6092-704c68dd-42d12661");

	ControlService(svc::ServiceLocator &svc);
	ControlService(ControlService &&) = delete;
	ControlService(const ControlService &) = delete;
	ControlService &operator=(ControlService &&) = delete;
	ControlService &operator=(const ControlService &) = delete;
	~ControlService() override;

	UID serviceUid() const noexcept override { return SERVICE_UID; }

	void asyncStartWorld(StartRequest req);
	void asyncSaveWorld(SaveRequest req);
	void asyncStopWorld(SaveRequest req);

	// Acquire a reference to the last complete (fully computed) world state.
	// Returns null pointer if there is no active connection to a world.
	// This function is thread-safe.
	std::shared_ptr<const State> getLastState() const;

private:
	extras::pimpl<detail::ControlServiceImpl, 32, 8> m_impl;
};

} // namespace voxen::world
