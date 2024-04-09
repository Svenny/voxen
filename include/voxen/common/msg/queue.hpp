#pragma once

#include <voxen/common/msg/config.hpp>
#include <voxen/common/msg/message.hpp>

#include <extras/function_ref.hpp>
#include <extras/hardware_params.hpp>
#include <extras/pimpl.hpp>

#include <optional>

namespace voxen::msg
{

// This is an unbounded (infinitely growing) multi-producer single-consumer queue.
// Only one thread can receive messages, calls from other threads will cause races.
// `send()` can be called from any thread (simultaneously with receiving as well).
// NOTE: this object is pretty huge (several full cachelines) and requires a full
// cacheline alignment (`extras::hardware_params::cache_line` bytes).
// NOTE: order of delivery is preserved only for single thread, not across threads.
class Queue final {
public:
	Queue();
	Queue(Queue &&) = delete;
	Queue(const Queue &) = delete;
	Queue &operator=(Queue &&) = delete;
	Queue &operator=(const Queue &) = delete;
	~Queue() noexcept;

	// Wrapper around the next `send()` method constructing message in place
	template<typename T, typename... Args>
	void send(uint32_t id, Args &&...args)
	{
		Message msg;
		msg.packPayload<T>(id, std::forward<Args>(args)...);
		send(msg);
	}

	// Put the message in queue. This method can be safely called by
	// multiple threads (even while other thread is receiving messages).
	void send(const Message &msg);
	// Call the provided callback on each message sent to this moment.
	// If handler throws an exception then draining process will stop, all
	// processed messages including failed one are removed from the queue.
	// NOTE: this method is inherently racey - there can be unhandled
	// messages after it returns (if new ones were sent during handling).
	// This will not happen with single-threaded or mutually exclusive access.
	// NOTE: this method can't be called by multiple threads simultaneously.
	void drain(extras::function_ref<void(Message &)> handler);
	// Extract one message sent to this moment. Returns `std::nullopt` if there are none.
	// NOTE: this method can't be called by multiple threads simultaneously.
	std::optional<Message> receiveOne();

private:
	class Shard;

	constexpr static size_t SHARD_SIZE = extras::hardware_params::cache_line;
	extras::pimpl<Shard, SHARD_SIZE, SHARD_SIZE> m_shards[Config::NUM_QUEUE_SHARDS];

	static uint32_t thisThreadShard() noexcept;
};

// Adapter class supporting only sending but not receiving messages.
// Implicitly constructible from `Queue`.
// This class must not outlive its referenced queue.
class QueueSender final {
public:
	constexpr QueueSender(Queue &queue) noexcept : m_queue(queue) {}

	template<typename T, typename... Args>
	void send(uint32_t id, Args &&...args)
	{
		m_queue.send<T>(id, std::forward<Args>(args)...);
	}

	void send(const Message &msg) { m_queue.send(msg); }

private:
	Queue &m_queue;
};

} // namespace voxen::msg
