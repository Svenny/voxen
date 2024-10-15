#pragma once

#include <extras/function_ref.hpp>

#include <utility>

namespace extras
{

enum class refcnt_ptr_action {
	acquire_ref,
	release_ref
};

// Reference-counter pointer to an object with type-erased lifecycle management function.
// Lifecycle management function must be thread-safe but object itself need not - that is,
// it's allowed to simultaneously have pointers to the same object in multiple threads, but
// access to the object is not synchronized by the pointer.
// NOTE: management functions may pose a certain limit on maximum count of references to an
// object. It may even be as low as 255. Exceeding this limit leads to undefined behavior.
// NOTE: management function's "context" object must outlive this pointer.
template<typename T>
class refcnt_ptr final {
public:
	using manager_type = function_ref<void(T *, refcnt_ptr_action) noexcept>;

	refcnt_ptr() = default;

	explicit refcnt_ptr(T *object, manager_type manager) noexcept : m_object(object), m_manager(manager) {}

	refcnt_ptr(refcnt_ptr &&other) noexcept
	{
		m_object = std::exchange(other.m_object, nullptr);
		m_manager = std::exchange(other.m_manager, {});
	}

	refcnt_ptr(const refcnt_ptr &other) noexcept : m_object(other.m_object), m_manager(other.m_manager)
	{
		acquire_ref();
	}

	refcnt_ptr &operator=(refcnt_ptr &&other) noexcept
	{
		std::swap(m_object, other.m_object);
		std::swap(m_manager, other.m_manager);
		return *this;
	}

	refcnt_ptr &operator=(const refcnt_ptr &other) noexcept
	{
		if (m_object == other.m_object) {
			// All refcnt logic is no-op if object is the same
			return *this;
		}

		release_ref();
		m_object = other.m_object;
		m_manager = other.m_manager;
		acquire_ref();
		return *this;
	}

	~refcnt_ptr() noexcept { release_ref(); }

	// Release managed object, pointer becomes null
	void reset() noexcept
	{
		release_ref();
		m_object = nullptr;
	}

	T *get() const noexcept { return m_object; }

	explicit operator bool() const noexcept { return m_object != nullptr; }
	std::add_lvalue_reference_t<T> operator*() const noexcept(noexcept(*m_object)) { return *m_object; }
	T *operator->() const noexcept { return m_object; }

private:
	void acquire_ref() noexcept
	{
		if (m_object) {
			m_manager(m_object, refcnt_ptr_action::acquire_ref);
		}
	}

	void release_ref() noexcept
	{
		if (m_object) {
			m_manager(m_object, refcnt_ptr_action::release_ref);
		}
	}

	T *m_object = nullptr;
	manager_type m_manager;
};

} // namespace extras
