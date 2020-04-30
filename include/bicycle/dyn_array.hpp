/* Just like std::vector, but non-resizable, i.e its size is fixed at
 * creation time. Fills in the gap between std::vector and std::array. */
#pragma once

#include <memory>
#include <stdexcept>

namespace bicycle
{

template<typename T, typename Allocator = std::allocator<T>>
class dyn_array {
public:
	using value_type = T;
	using allocator_type = Allocator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;
	// TODO: add iterators
	using iterator = void;
	using const_iterator = const void;
	using reverse_iterator =  std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	explicit dyn_array(size_type count, const T &value, const Allocator &alloc = Allocator())
	   : m_size(count), m_alloc(alloc) {
		m_data = std::allocator_traits<Allocator>::allocate(m_alloc, count);
		std::uninitialized_fill_n(m_data, count, value);
	}
	explicit dyn_array(size_type count, const Allocator &alloc = Allocator())
	   : m_size(count), m_alloc(alloc) {
		m_data = std::allocator_traits<Allocator>::allocate(m_alloc, count);
	}
	template<typename InputIt>
	dyn_array(InputIt first, InputIt last, const Allocator &alloc = Allocator());
	dyn_array(const dyn_array &other);
	dyn_array(const dyn_array &other, const Allocator &alloc);
	dyn_array(dyn_array &&other) noexcept;
	dyn_array(dyn_array &&other, const Allocator &alloc);
	dyn_array(std::initializer_list<T> init, const Allocator &alloc = Allocator());

	~dyn_array() {
		std::allocator_traits<Allocator>::deallocate(m_alloc, m_data, m_size);
	}

	T &at(size_type pos) {
		if (pos >= m_size)
			throw std::out_of_range("bicycle::dyn_array::at() failed bounds check");
		return m_data[pos];
	}

	const T &at(size_type pos) const {
		if (pos >= m_size)
			throw std::out_of_range("bicycle::dyn_array::at() failed bounds check");
		return m_data[pos];
	}

	T &operator[](size_type pos) { return m_data[pos]; }
	const T &operator[](size_type pos) const { return m_data[pos]; }

	constexpr T *data() noexcept { return m_data; }
	constexpr const T *data() const noexcept { return m_data; }

	[[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }
	[[nodiscard]] constexpr size_type size() const noexcept { return m_size; }

private:
	const size_type m_size;
	T *m_data;
	Allocator m_alloc;
};

// Deduction guide for constructing from two iterators
template<typename InputIt, typename Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>>
dyn_array(InputIt, InputIt, Alloc = Alloc())
   -> dyn_array<typename std::iterator_traits<InputIt>::value_type, Alloc>;

}
