/* Just like std::vector, but non-resizable, i.e its size is fixed at
 * creation time. Fills in the gap between std::vector and std::array. */
#pragma once

#include <algorithm>
#include <iterator>
#include <memory>
#include <span>

namespace extras
{

template<typename T>
class dyn_array_iterator : public std::contiguous_iterator_tag {
public:
	using value_type = T;
	using difference_type = ptrdiff_t;
	using pointer = T *;
	using reference = T &;
	using iterator_category = std::contiguous_iterator_tag;

	constexpr dyn_array_iterator() noexcept {}
	constexpr dyn_array_iterator(std::add_const_t<T> *ptr) noexcept : m_ptr(ptr) {}
	constexpr dyn_array_iterator(std::remove_const_t<T> *ptr) noexcept : m_ptr(ptr) {}

	constexpr T &operator*() const noexcept { return *m_ptr; }
	constexpr T *operator->() const noexcept { return m_ptr; }
	constexpr T &operator[](ptrdiff_t diff) const noexcept { return m_ptr[diff]; }

	constexpr bool operator==(const dyn_array_iterator &other) const noexcept { return m_ptr == other.m_ptr; }
	constexpr bool operator!=(const dyn_array_iterator &other) const noexcept { return m_ptr != other.m_ptr; }
	constexpr bool operator<(const dyn_array_iterator &other) const noexcept { return m_ptr < other.m_ptr; }
	constexpr bool operator>(const dyn_array_iterator &other) const noexcept { return m_ptr > other.m_ptr; }
	constexpr bool operator<=(const dyn_array_iterator &other) const noexcept { return m_ptr <= other.m_ptr; }
	constexpr bool operator>=(const dyn_array_iterator &other) const noexcept { return m_ptr >= other.m_ptr; }

	constexpr ptrdiff_t operator-(const dyn_array_iterator &other) const noexcept { return m_ptr - other.m_ptr; }
	constexpr dyn_array_iterator operator-(ptrdiff_t diff) const noexcept { return dyn_array_iterator(m_ptr - diff); }
	constexpr dyn_array_iterator operator+(ptrdiff_t diff) const noexcept { return dyn_array_iterator(m_ptr + diff); }

	constexpr friend dyn_array_iterator operator+(ptrdiff_t diff, const dyn_array_iterator &iter) noexcept
	{
		return iter + diff;
	}

	constexpr dyn_array_iterator &operator+=(ptrdiff_t diff) noexcept
	{
		m_ptr += diff;
		return *this;
	}

	constexpr dyn_array_iterator &operator-=(ptrdiff_t diff) noexcept
	{
		m_ptr -= diff;
		return *this;
	}

	constexpr dyn_array_iterator &operator++() noexcept
	{
		m_ptr++;
		return *this;
	}

	constexpr dyn_array_iterator &operator--() noexcept
	{
		m_ptr--;
		return *this;
	}

	constexpr dyn_array_iterator operator++(int) noexcept
	{
		dyn_array_iterator old_me(m_ptr);
		m_ptr++;
		return old_me;
	}

	constexpr dyn_array_iterator operator--(int) noexcept
	{
		dyn_array_iterator old_me(m_ptr);
		m_ptr--;
		return old_me;
	}

private:
	T *m_ptr = nullptr;
};

template<typename T, typename Allocator = std::allocator<T>>
class dyn_array {
public:
	// Enforce Voxen design guidelines to some extent (and get rid of ugly edge cases in implementation)
	static_assert(std::is_nothrow_destructible_v<T>, "extras::dyn_array doesn't allow throwing destructors");
	static_assert(!std::is_move_constructible_v<T> || std::is_nothrow_move_constructible_v<T>,
		"extras::dyn_array doesn't allow throwing move constructors");
	static_assert(!std::is_move_assignable_v<T> || std::is_nothrow_move_assignable_v<T>,
		"extras::dyn_array doesn't allow throwing move assignment");
	// Protect from esoteric edge cases with allocators
	static_assert(std::is_nothrow_destructible_v<Allocator> && std::is_nothrow_copy_constructible_v<Allocator>
			&& std::is_nothrow_move_constructible_v<Allocator> && std::is_nothrow_copy_assignable_v<Allocator>
			&& std::is_nothrow_move_assignable_v<Allocator>,
		"extras::dyn_array doesn't allow throwing allocator");

	using value_type = T;
	using allocator_type = Allocator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;

	using iterator = dyn_array_iterator<T>;
	using const_iterator = dyn_array_iterator<const T>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr dyn_array() noexcept(noexcept(Allocator())) {}

	constexpr explicit dyn_array(const Allocator &alloc) noexcept : m_alloc(alloc) {}

	constexpr explicit dyn_array(size_type count, const T &value, const Allocator &alloc = Allocator()) : m_alloc(alloc)
	{
		uninitialized_construct(count, [&value](void *place, size_type) { ::new (place) T(value); });
	}

	/// This constructor with owning changing from raw allocated memory.
	constexpr explicit dyn_array(T *data, size_type count, const Allocator &alloc) noexcept
		: m_size(count), m_data(data), m_alloc(alloc)
	{
		// do nothing
	}

	constexpr explicit dyn_array(size_type count, const Allocator &alloc = Allocator()) : m_alloc(alloc)
	{
		uninitialized_construct(count, [](void *place, size_type) { ::new (place) T(); });
	}

	// Generating constructor - generates items by calling `generator(void *place, size_type index)`
	// `place` points to uninitialized bytes; called in strict order `index = 0, 1, 2, ...`.
	// NOTE: `generator` acts as a "programmable placement new" - i.e. object is assumed
	// constructed (lifetime started) if it returns normally; and not constructed if it throws.
	template<typename F>
	constexpr explicit dyn_array(size_type count, F &&generator, const Allocator &alloc = Allocator())
		requires(
			!std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<F>> && std::is_invocable_v<F, void *, size_type>)
		: m_alloc(alloc)
	{
		uninitialized_construct(count, std::forward<F>(generator));
	}

	template<std::forward_iterator InputIt>
	constexpr explicit dyn_array(InputIt first, InputIt last, const Allocator &alloc = Allocator()) : m_alloc(alloc)
	{
		uninitialized_construct(static_cast<size_t>(std::distance(first, last)), [&first](void *place, size_type) {
			::new (place) T(*first);
			++first;
		});
	}

	constexpr dyn_array(const dyn_array &other) : m_size(other.m_size), m_alloc(other.m_alloc)
	{
		uninitialized_construct(other.m_size, [&other](void *place, size_type index) { ::new (place) T(other[index]); });
	}

	constexpr dyn_array(dyn_array &&other) noexcept
		: m_size(other.m_size), m_data(other.m_data), m_alloc(std::move(other.m_alloc))
	{
		other.m_size = 0;
		other.m_data = nullptr;
	}

	constexpr dyn_array(std::initializer_list<T> init, const Allocator &alloc = Allocator())
		: dyn_array(init.begin(), init.end(), alloc)
	{}

	constexpr dyn_array &operator=(const dyn_array &other)
	{
		// Don't reallocate when size/allocator is not changing
		if (m_size == other.m_size && m_alloc == other.m_alloc) {
			std::copy(other.cbegin(), other.cend(), begin());
			return *this;
		}

		return *this = dyn_array { other };
	}

	constexpr dyn_array &operator=(dyn_array &&other) noexcept
	{
		std::swap(m_size, other.m_size);
		std::swap(m_data, other.m_data);
		std::swap(m_alloc, other.m_alloc);
		return *this;
	}

	~dyn_array() noexcept
	{
		if constexpr (!std::is_trivially_destructible_v<T>) {
			std::destroy_n(m_data, m_size);
		}
		std::allocator_traits<Allocator>::deallocate(m_alloc, m_data, m_size);
	}

	[[nodiscard]] std::span<const std::byte> as_bytes() const noexcept
	{
		return std::as_bytes(std::span<const T, std::dynamic_extent>(m_data, m_size));
	}

	[[nodiscard]] std::span<std::byte> as_writable_bytes() noexcept
	{
		return std::as_writable_bytes(std::span<T, std::dynamic_extent>(m_data, m_size));
	}

	operator std::span<const T>() const noexcept { return { m_data, m_size }; }
	operator std::span<T>() noexcept { return { m_data, m_size }; }

	[[nodiscard]] constexpr T &operator[](size_type pos) noexcept { return m_data[pos]; }
	[[nodiscard]] constexpr const T &operator[](size_type pos) const noexcept { return m_data[pos]; }

	[[nodiscard]] constexpr T *data() noexcept { return m_data; }
	[[nodiscard]] constexpr const T *data() const noexcept { return m_data; }

	[[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }
	[[nodiscard]] constexpr size_type size() const noexcept { return m_size; }
	[[nodiscard]] constexpr size_type size_bytes() const noexcept { return m_size * sizeof(T); }

	[[nodiscard]] constexpr iterator begin() noexcept { return iterator(m_data); }
	[[nodiscard]] constexpr const_iterator begin() const noexcept { return const_iterator(m_data); }
	[[nodiscard]] constexpr const_iterator cbegin() const noexcept { return const_iterator(m_data); }

	[[nodiscard]] constexpr iterator end() noexcept { return iterator(m_data + m_size); }
	[[nodiscard]] constexpr const_iterator end() const noexcept { return const_iterator(m_data + m_size); }
	[[nodiscard]] constexpr const_iterator cend() const noexcept { return const_iterator(m_data + m_size); }

private:
	size_type m_size = 0;
	T *m_data = nullptr;
	[[no_unique_address]] Allocator m_alloc = {};

	template<typename F>
	void uninitialized_construct(size_type count, F &&generator)
	{
		size_type init = 0;
		T *data = std::allocator_traits<Allocator>::allocate(m_alloc, count);

		try {
			while (init < count) {
				generator(static_cast<void *>(data + init), init);
				++init;
			}

			m_size = count;
			m_data = data;
		}
		catch (...) {
			if constexpr (!std::is_trivially_destructible_v<T>) {
				std::destroy_n(data, init);
			}

			std::allocator_traits<Allocator>::deallocate(m_alloc, data, count);
			throw;
		}
	}
};

// Deduction guide for constructing from two iterators
template<typename InputIt, typename Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>>
dyn_array(InputIt, InputIt, Alloc = Alloc()) -> dyn_array<typename std::iterator_traits<InputIt>::value_type, Alloc>;

} // namespace extras
