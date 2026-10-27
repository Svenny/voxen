#pragma once

#include <voxen/svc/svc_fwd.hpp>

#include <cstddef>

namespace voxen::svc::detail
{

class TaskServiceSlave {
public:
	static void threadFn(TaskService &my_service, size_t my_queue, AsyncCounterTracker &counter_tracker,
		TaskQueueSet &queue_set);
};

} // namespace voxen::svc::detail
