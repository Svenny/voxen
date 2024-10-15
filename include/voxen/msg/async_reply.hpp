#pragma once

#include <voxen/msg/message_payload.hpp>
#include <voxen/visibility.hpp>

#include <chrono>
#include <system_error>

namespace voxen::msg
{

class VOXEN_API AsyncReplyBase {
public:
	void wait();
	template<typename Rep, typename Period>
	void waitFor(const std::chrono::duration<Rep, Period> &rel_time);
	template<typename Clock, typename Duration>
	void waitUntil(const std::chrono::time_point<Clock, Duration> &timeout_time);

	bool ready() const noexcept;
	bool replyStatus() const noexcept;
	std::error_condition replyCode() const noexcept;

protected:
	void *getPayloadPtr() const noexcept;
};

template<MessagePayload P>
class VOXEN_API AsyncReply : public AsyncReplyBase {
public:
	P &payload() noexcept { return reinterpret_cast<P &>(getPayloadPtr()); }
	const P &payload() const noexcept { return reinterpret_cast<const P &>(getPayloadPtr()); }
};

} // namespace voxen::msg
