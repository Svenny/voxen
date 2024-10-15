#pragma once

#include <concepts>
#include <type_traits>

namespace voxen::msg
{

class IMessagePayload {
public:
	virtual ~IMessagePayload() noexcept = 0;
};

template<typename T>
concept MessagePayload = (std::is_trivially_destructible_v<T>
	|| std::derived_from<T, IMessagePayload>) &&std::is_nothrow_move_constructible_v<T>;

template<typename T>
concept CopyableMessagePayload = MessagePayload<T> && std::is_copy_constructible_v<T>;

} // namespace voxen::msg
