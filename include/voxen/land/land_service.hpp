#pragma once

#include <voxen/land/land_fwd.hpp>
#include <voxen/svc/service_base.hpp>
#include <voxen/uid.hpp>
#include <voxen/util/tick_id.hpp>
#include <voxen/visibility.hpp>

#include <extras/pimpl.hpp>

namespace voxen::land
{

// Accepts the following messages:
// - `voxen::land::ChunkTicketRequestMessage`
class VOXEN_API LandService final : public svc::IService {
public:
	constexpr static UID SERVICE_UID = UID("bbefcea8-8ef334a9-89dd1efc-c0176d14");

	explicit LandService(svc::ServiceLocator &svc);
	LandService(LandService &&) = delete;
	LandService(const LandService &) = delete;
	LandService &operator=(LandService &&) = delete;
	LandService &operator=(const LandService &) = delete;
	~LandService() override;

	UID serviceUid() const noexcept override { return SERVICE_UID; }

	void doTick(WorldTickId tick_id);
	const LandState &stateForCopy() const noexcept;

	// Make all pending and future land generation jobs exit immediately without doing
	// any work. Intended to be called once the world is stopping: otherwise its save
	// task (and our destructor) would wait for the whole generation backlog, which can
	// take tens of seconds in debug builds. Block edits are not cancelled.
	// This is irreversible, land state will not be updated properly after the call.
	// Thread-safe, can be called concurrently with any other function.
	void cancelGenerationJobs() noexcept;

private:
	extras::pimpl<detail::LandServiceImpl, 2048, 8> m_impl;
};

} // namespace voxen::land
