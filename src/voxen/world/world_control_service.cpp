#include <voxen/world/world_control_service.hpp>

#include <voxen/debug/uid_registry.hpp>
#include <voxen/land/land_service.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/util/log.hpp>

#include "world_sim_thread.hpp"

namespace voxen::world
{

namespace
{

auto makeLandService(svc::ServiceLocator &svc)
{
	return std::make_unique<land::LandService>(svc);
}

} // namespace

class detail::ControlServiceImpl {
public:
	ControlServiceImpl(svc::ServiceLocator &svc) : m_svc(svc)
	{
		// TODO: not quite right to put it here?
		// I think `LandService` shouldn't even be a service in the first place.
		debug::UidRegistry::registerLiteral(land::LandService::SERVICE_UID, "voxen::land::LandService");
		svc.registerServiceFactory<land::LandService>(makeLandService);
	}

	~ControlServiceImpl()
	{
		if (m_sim_thread) {
			Log::warn("world::ControlService stopping with an established world connection! Auto-saving it.");
			stop({});
		}
	}

	void start(ControlService::StartRequest req)
	{
		// Already started
		if (m_sim_thread) {
			Log::warn("Attempt to start world with an already established connection");

			if (req.result_callback) {
				req.result_callback(std::errc::already_connected);
			}

			return;
		}

		m_sim_thread = SimThread::create(m_svc, std::move(req));
	}

	void save(ControlService::SaveRequest req)
	{
		// No world connection
		if (!m_sim_thread) {
			Log::warn("Attempt to save world without an established connection");

			if (req.result_callback) {
				req.result_callback(std::errc::not_connected);
			}

			return;
		}

		m_sim_thread->requestSave(std::move(req));
	}

	void stop(ControlService::SaveRequest req)
	{
		// No world connection
		if (!m_sim_thread) {
			Log::warn("Attempt to stop world without an established connection");

			if (req.result_callback) {
				req.result_callback(std::errc::not_connected);
			}

			return;
		}

		m_sim_thread->requestStop(std::move(req));
		m_sim_thread.reset();
	}

	std::shared_ptr<const State> getLastState() const { return m_sim_thread ? m_sim_thread->getLastState() : nullptr; }

private:
	svc::ServiceLocator &m_svc;

	std::shared_ptr<SimThread> m_sim_thread;
};

ControlService::ControlService(svc::ServiceLocator &svc) : m_impl(svc) {}

ControlService::~ControlService() = default;

void ControlService::asyncStartWorld(StartRequest req)
{
	m_impl->start(std::move(req));
}

void ControlService::asyncSaveWorld(SaveRequest req)
{
	m_impl->save(std::move(req));
}

void ControlService::asyncStopWorld(SaveRequest req)
{
	m_impl->stop(std::move(req));
}

std::shared_ptr<const State> ControlService::getLastState() const
{
	return m_impl->getLastState();
}

} // namespace voxen::world
