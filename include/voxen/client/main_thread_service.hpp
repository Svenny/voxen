#pragma once

#include <voxen/gfx/ui/ui_fwd.hpp>
#include <voxen/svc/service_base.hpp>
#include <voxen/visibility.hpp>

#include <extras/function_ref.hpp>
#include <extras/pimpl.hpp>

namespace voxen::client
{

// NOTE: this service must not be started on non-main thread.
// It's better to explicitly request it from `main()` function.
class VOXEN_API MainThreadService final : public svc::IService {
public:
	struct FrameCallbackData {
		double delta_time;

		gfx::ui::UiBuilder &ui_builder;
	};

	using FrameCallback = extras::function_ref<bool(FrameCallbackData)>;

	constexpr static UID SERVICE_UID = UID("b005e5b0-5c3538ae-b9cee98d-90a07351");

	struct Config {};

	MainThreadService(svc::ServiceLocator &svc, Config cfg);
	MainThreadService(MainThreadService &&) = delete;
	MainThreadService(const MainThreadService &) = delete;
	MainThreadService &operator=(MainThreadService &&) = delete;
	MainThreadService &operator=(const MainThreadService &) = delete;
	~MainThreadService() noexcept override;

	UID serviceUid() const noexcept override { return SERVICE_UID; }

	void doMainLoop(FrameCallback frame_callback);

private:
	struct Impl;
	extras::pimpl<Impl, 160, 8> m_impl;
};

} // namespace voxen::client
