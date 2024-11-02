#include <voxen/svc/message_handling.hpp>

#include <voxen/os/futex.hpp>

#include "messaging_private.hpp"

#include <atomic>
#include <cassert>
#include <utility>

namespace voxen::svc
{

namespace
{

RequestStatus atomicWordToStatus(uint32_t word) noexcept
{
	// See `MessageAuxData::atomic_word` for bitfields descriptions
	return static_cast<RequestStatus>((word >> 17) & 0b11);
}

} // namespace

UID MessageInfo::senderUid() const noexcept
{
	return m_hdr->from_uid;
}

RequestStatus RequestCompletionInfo::status() const noexcept
{
	// Relaxed order - no synchronization is needed anymore
	return atomicWordToStatus(m_hdr->aux_data.atomic_word.load(std::memory_order_relaxed));
}

void RequestCompletionInfo::rethrowIfFailed()
{
	// Not checking status - non-null exception can only happen in Failed status
	if (m_hdr->requestBlock()->exception) {
		std::rethrow_exception(std::move(m_hdr->requestBlock()->exception));
	}
}

RequestHandleBase::RequestHandleBase(detail::MessageHeader* hdr) noexcept : m_hdr(hdr)
{
	// Add refcount stored in low 16 bits of atomic word.
	// Relaxed - we're creating this handle as part of message allocation.
	hdr->aux_data.atomic_word.fetch_add(1, std::memory_order_relaxed);
}

RequestHandleBase::RequestHandleBase(RequestHandleBase&& other) noexcept : m_hdr(std::exchange(other.m_hdr, nullptr)) {}

RequestHandleBase& RequestHandleBase::operator=(RequestHandleBase&& other) noexcept
{
	std::swap(m_hdr, other.m_hdr);
	return *this;
}

RequestHandleBase::~RequestHandleBase() noexcept
{
	reset();
}

void RequestHandleBase::reset() noexcept
{
	if (m_hdr) {
		m_hdr->releaseRef();
		m_hdr = nullptr;
	}
}

void* RequestHandleBase::payload() noexcept
{
	assert(m_hdr);
	return m_hdr->payload();
}

RequestStatus RequestHandleBase::wait() noexcept
{
	assert(m_hdr);

	uint32_t word = m_hdr->aux_data.atomic_word.load(std::memory_order_acquire);
	auto st = atomicWordToStatus(word);
	if (st != RequestStatus::Pending) {
		return st;
	}

	// Set futex waiting flag. We don't need to care about resetting it,
	// there will be no more waits - this one is until completion.
	// See `MessageAuxData::atomic_word` for bitfields description.
	word = m_hdr->aux_data.atomic_word.fetch_or(1u << 16, std::memory_order_release);
	// Don't forget to recalculate status after fetching a new value
	st = atomicWordToStatus(word);

	while (st == RequestStatus::Pending) {
		os::Futex::waitInfinite(&m_hdr->aux_data.atomic_word, word);
		// Reload value after returning from wait
		word = m_hdr->aux_data.atomic_word.load(std::memory_order_acquire);
		st = atomicWordToStatus(word);
	}

	return st;
}

RequestStatus RequestHandleBase::status() noexcept
{
	assert(m_hdr);
	return atomicWordToStatus(m_hdr->aux_data.atomic_word.load(std::memory_order_acquire));
}

void RequestHandleBase::rethrowIfFailed()
{
	assert(m_hdr);
	if (status() == RequestStatus::Failed && m_hdr->requestBlock()->exception) {
		std::rethrow_exception(std::move(m_hdr->requestBlock()->exception));
	}
}

} // namespace voxen::svc
