#pragma once

#include <voxen/common/msg/config.hpp>

#include <new>
#include <type_traits>
#include <utility>

namespace voxen::msg
{

using MessagePayloadStore = std::aligned_storage_t<Config::MESSAGE_SIZE - sizeof(uint64_t), alignof(uint64_t)>;

template<typename T>
concept MessagePayload = std::is_void_v<T>
	|| (std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(MessagePayloadStore)
		&& alignof(T) <= alignof(MessagePayloadStore));

// This is a trivially copyable storage for messages. Any trivially copyable type can be
// packed into it. Note that validity of ID-payload type correspondence is not checked,
// this container is "dumb" with regards to type safety.
class Message final {
public:
	Message() = default;
	Message(Message &&) = default;
	Message(const Message &) = default;
	Message &operator=(Message &&) = default;
	Message &operator=(const Message &) = default;
	~Message() = default;

	uint32_t id() const noexcept { return m_id; }

	template<MessagePayload T>
	T &getPayload() noexcept
	{
		return *std::launder(reinterpret_cast<T *>(&m_payload));
	}

	template<MessagePayload T>
	const T &getPayload() const noexcept
	{
		return *std::launder(reinterpret_cast<const T *>(&m_payload));
	}

	// Store ID and construct an object in payload store.
	// As payload types are trivially copyable there is no need to destruct previous payload.
	// Enforcing type safety should be done externally by higher-level entities.
	template<MessagePayload T, typename... Args>
	void packPayload(uint32_t id, Args &&...args) noexcept(noexcept(T(std::forward<Args>(args)...)))
	{
		m_id = id;

		if constexpr (!std::is_void_v<T>) {
			new (&m_payload) T(std::forward<Args>(args)...);
		}
	}

private:
	uint32_t m_id = 0;
	// [4-byte padding]
	MessagePayloadStore m_payload;
};

} // namespace voxen::msg
