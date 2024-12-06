#pragma once

namespace voxen::svc
{

class Engine;
class IService;
class MessageQueue;
class MessageSender;
class MessagingService;
class ServiceLocator;
class TaskBuilder;
class TaskContext;
class TaskHandle;
class TaskService;

namespace detail
{

class PrivateTaskHandle;
class TaskCounterTracker;
struct TaskHeader;
class TaskQueueSet;
class TaskServiceImpl;

} // namespace detail

} // namespace voxen::svc
