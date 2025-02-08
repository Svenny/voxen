#pragma once

#include <voxen/land/land_fwd.hpp>
#include <voxen/svc/service_base.hpp>
#include <voxen/world/world_tick_id.hpp>

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

	void doTick(world::TickId tick_id);
	const LandState &stateForCopy() const noexcept;

private:
	extras::pimpl<detail::LandServiceImpl, 2048, 8> m_impl;
};

} // namespace voxen::land
