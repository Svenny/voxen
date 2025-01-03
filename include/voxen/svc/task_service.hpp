#pragma once

#include <voxen/svc/service_base.hpp>
#include <voxen/svc/svc_fwd.hpp>

#include <extras/pimpl.hpp>

#include <span>

namespace voxen::svc
{

class VOXEN_API TaskService : public IService {
public:
	constexpr static UID SERVICE_UID = UID("28786522-a1076eb8-12aeb24a-53f130ca");

	struct Config {
		size_t num_threads = 0;
	};

	TaskService(ServiceLocator &svc, Config cfg);
	TaskService(TaskService &&) = delete;
	TaskService(const TaskService &) = delete;
	TaskService &operator=(TaskService &&) = delete;
	TaskService &operator=(const TaskService &) = delete;
	~TaskService() override;

	UID serviceUid() const noexcept override { return SERVICE_UID; }

	// Check a set of task counters for completion and remove completed ones.
	// Returns the number of remaining incomplete counters - they will be moved
	// to the first consecutive elements of `counters` in unspecified order.
	// The remaining elements of `counters` will have undefined (garbage) values.
	size_t eliminateCompletedWaitCounters(std::span<uint64_t> counters) noexcept;

private:
	extras::pimpl<detail::TaskServiceImpl, 64, 8> m_impl;

	uint64_t enqueueTask(detail::PrivateTaskHandle handle);

	friend class TaskBuilder;
};

} // namespace voxen::svc
