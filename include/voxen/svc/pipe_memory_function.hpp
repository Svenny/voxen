#pragma once

#include <voxen/common/pipe_memory_allocator.hpp>

#include <concepts>
#include <type_traits>

namespace voxen::svc
{

// Provides storage for a callable object backed by `PipeMemoryAllocator`.
// Intended to store completion handler lambdas for asynchronous operations.
// Modeled after `std::move_only_function` with unncesessary features stripped.
//
// In most cases you should not create this object directly. Simply pass your lambda
// as argument to the asynchronous operation and let implicit constructor do its job.
//
// `PipeMemoryAllocator` service must be started before any such function is created
// and must not be stopped while any of them is alive. This should never be a problem
// if you only use it as intended, i.e. implicitly create in asynchronous operations.
template<typename... Signature>
class PipeMemoryFunction;

namespace detail
{

template<typename T>
constexpr bool IS_PIPE_MEMORY_FUNCTION = false;

template<typename T>
constexpr bool IS_PIPE_MEMORY_FUNCTION<PipeMemoryFunction<T>> = true;

} // namespace detail

// This concept models a callable object suitable for storage in `PipeMemoryFunction`.
// Its idea is to only allow lambdas and lambda-like objects to keep implementation as simple
// as possible; and probably reduce compile times too - we're gonna instantiate this a lot.
//
// To avoid accidental double wrapping, `PipeMemoryObject` itself cannot be stored there.
template<typename T, typename Res, typename... Args>
concept CPipeMemoryFunctionLambda = std::is_class_v<std::remove_reference_t<T>> && std::is_nothrow_destructible_v<T>
	&& std::is_invocable_r_v<Res, T, Args...> && !detail::IS_PIPE_MEMORY_FUNCTION<T>;

template<typename Res, typename... Args>
class PipeMemoryFunction<Res(Args...)> {
public:
	PipeMemoryFunction() = default;

	template<CPipeMemoryFunctionLambda<Res, Args...> Fn>
	PipeMemoryFunction(Fn &&fn)
	{
		using ST = Storage<Fn>;
		m_storage = PipeMemoryAllocator::make<ST>(std::forward<Fn>(fn));
		m_storage->dtor = [](void *thiz) noexcept { reinterpret_cast<ST *>(thiz)->~ST(); };
		m_storage->invoker = [](void *thiz, TParam<Args>... args) -> Res {
			// Have no `std::invoke_r<Res>` before C++23
			if constexpr (std::is_void_v<Res>) {
				reinterpret_cast<ST *>(thiz)->object(std::forward<TParam<Args>>(args)...);
			} else {
				return reinterpret_cast<ST *>(thiz)->object(std::forward<TParam<Args>>(args)...);
			}
		};
	}

	PipeMemoryFunction(PipeMemoryFunction &&other) noexcept : m_storage(std::exchange(other.m_storage, nullptr)) {}

	PipeMemoryFunction &operator=(PipeMemoryFunction &&other) noexcept
	{
		std::swap(m_storage, other.m_storage);
		return *this;
	}

	~PipeMemoryFunction() noexcept
	{
		if (m_storage) {
			m_storage->dtor(m_storage);
			PipeMemoryAllocator::deallocate(m_storage);
		}
	}

	PipeMemoryFunction(const PipeMemoryFunction &) = delete;
	PipeMemoryFunction &operator=(const PipeMemoryFunction &) = delete;

	operator bool() const noexcept { return m_storage != nullptr; }

	Res operator()(Args... args) { return m_storage->invoker(m_storage, std::forward<Args>(args)...); }

private:
	// Pass scalar parameters directly by value, forward other ones
	template<typename T>
	using TParam = std::conditional_t<std::is_scalar_v<T>, T, T &&>;

	using TDtor = void (*)(void *thiz) noexcept;
	using TInvoker = Res (*)(void *thiz, TParam<Args>...);

	struct StorageHeader {
		TDtor dtor = nullptr;
		TInvoker invoker = nullptr;
	};

	template<CPipeMemoryFunctionLambda<Res, Args...> Fn>
	struct Storage : StorageHeader {
		Storage(Fn &&fn) : object(std::forward<Fn>(fn)) {}
		Fn object;
	};

	StorageHeader *m_storage = nullptr;
};

} // namespace voxen::svc
