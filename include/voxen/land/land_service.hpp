#pragma once

#include <voxen/common/world_state.hpp>
#include <voxen/svc/service_base.hpp>

#include <extras/pimpl.hpp>

namespace voxen::land
{

namespace detail
{

class LandServiceImpl;

}

struct LandState;

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

private:
	extras::pimpl<detail::LandServiceImpl, 2048, 8> m_impl;
};

} // namespace voxen::land
