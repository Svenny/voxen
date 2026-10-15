#pragma once

namespace voxen::svc
{

class AssetFileRef;
class AssetService;
template<typename>
class CoroFuture;
template<typename>
class CoroSubTask;
class CoroTask;
class Engine;
class IAsset;
class IAssetDecoder;
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

class AsyncCounterTracker;
class AsyncFileIoServiceImpl;
class CoroSubTaskStateBase;
template<typename>
class CoroSubTaskState;
class CoroTaskState;
class PrivateTaskHandle;
struct TaskHeader;
class TaskQueueSet;
class TaskServiceImpl;

} // namespace detail

} // namespace voxen::svc
