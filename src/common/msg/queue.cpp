#include <voxen/common/msg/queue.hpp>

#include <voxen/os/futex.hpp>

#include <mutex>
#include <thread>

namespace voxen::msg
{

class Queue::Shard {
public:
	Shard()
	{
		Segment *segment = new Segment;
		m_consume_segment = segment;
		m_produce_segment = segment;
		m_last_segment = segment;
	}

	Shard(Shard &&) = delete;
	Shard(const Shard &) = delete;
	Shard &operator=(Shard &&) = delete;
	Shard &operator=(const Shard &) = delete;

	~Shard() noexcept
	{
		Segment *segment = m_consume_segment;

		while (segment) {
			Segment *next = segment->next;
			delete segment;
			segment = next;
		}
	}

	// Returns `true` if at least one message was consumed.
	// Consumes only up to one segment, so multiple calls may
	// be required to consume everything.
	bool consume(extras::function_ref<void(Message &)> handler)
	{
		if (!resetFullyConsumedSegment()) {
			return false;
		}

		uint32_t produce_pos = m_consume_segment->produce_pos.load(std::memory_order_acquire);
		bool consumed = false;

		while (m_consume_pos < produce_pos) {
			consumed = true;

			// Advance consumed counter before handling so if handler
			// throws the message will still be removed from queue
			Message &msg = m_consume_segment->data[m_consume_pos];
			m_consume_pos++;

			handler(msg);
		}

		return consumed;
	}

	// Extract one message from queue
	std::optional<Message> receiveOne()
	{
		if (!resetFullyConsumedSegment()) {
			return std::nullopt;
		}

		if (m_consume_pos < m_consume_segment->produce_pos.load(std::memory_order_acquire)) {
			return m_consume_segment->data[m_consume_pos++];
		}

		return std::nullopt;
	}

	// Put a message in queue
	void produce(const Message &msg)
	{
		std::lock_guard lock(m_lock);

		Segment *segment = m_produce_segment;
		uint32_t produce_pos = segment->produce_pos.load(std::memory_order_relaxed);

		if (produce_pos == Config::QUEUE_SEGMENT_SIZE) [[unlikely]] {
			segment = segment->next;
			produce_pos = 0;

			if (!segment) [[unlikely]] {
				Segment *new_segment = new Segment;
				new_segment->produce_pos.store(1, std::memory_order_relaxed);
				new_segment->data[0] = msg;

				m_produce_segment = new_segment;

				m_last_segment->next = new_segment;
				m_last_segment = new_segment;
				return;
			}

			m_produce_segment = segment;
		}

		segment->data[produce_pos] = msg;
		segment->produce_pos.store(produce_pos + 1, std::memory_order_release);
	}

private:
	struct Segment {
		Segment *next = nullptr;
		std::atomic<uint32_t> produce_pos = 0;
		Message data[Config::QUEUE_SEGMENT_SIZE];
	};

	os::FutexLock m_lock;
	uint32_t m_consume_pos = 0;
	Segment *m_consume_segment = nullptr;
	Segment *m_produce_segment = nullptr;
	Segment *m_last_segment = nullptr;

	// Check if consume head segment is fully consumed, reset
	// and move to the queue tail if it is. Will not reset/move
	// if there is no next consumable segment.
	// Returns `true` if consume head segment is not fully consumed.
	// If `false` is returned it is guaranteed there are no messages.
	bool resetFullyConsumedSegment() noexcept
	{
		if (m_consume_pos == Config::QUEUE_SEGMENT_SIZE) {
			std::lock_guard lock(m_lock);

			Segment *segment = m_consume_segment;
			if (segment == m_produce_segment) [[unlikely]] {
				return false;
			}

			Segment *next = segment->next;
			if (!next) [[unlikely]] {
				return false;
			}

			segment->next = nullptr;
			segment->produce_pos.store(0, std::memory_order_relaxed);

			m_consume_segment = next;
			m_consume_pos = 0;

			m_last_segment->next = segment;
			m_last_segment = segment;
		}

		return true;
	}
};

Queue::Queue() = default;
Queue::~Queue() noexcept = default;

void Queue::send(const Message &msg)
{
	m_shards[thisThreadShard()].object().produce(msg);
}

void Queue::drain(extras::function_ref<void(Message &)> handler)
{
	for (auto &pshard : m_shards) {
		Shard &shard = pshard.object();

		while (shard.consume(handler)) {
			// Will repeat until shard is empty
		}
	}
}

std::optional<Message> Queue::receiveOne()
{
	for (auto &pshard : m_shards) {
		auto opt = pshard.object().receiveOne();
		if (opt.has_value()) {
			return opt;
		}
	}

	return std::nullopt;
}

uint32_t Queue::thisThreadShard() noexcept
{
	auto id = std::this_thread::get_id();
	size_t hash = std::hash<std::thread::id>()(id);
	return hash % Config::NUM_QUEUE_SHARDS;
}

} // namespace voxen::msg
