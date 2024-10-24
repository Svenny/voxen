#ifndef _EXTRAS_MOFUNC_CV
	#define _EXTRAS_MOFUNC_CV
#endif

#ifdef _EXTRAS_MOFUNC_REF
	#define _EXTRAS_MOFUNC_INV_QUALS _EXTRAS_MOFUNC_CV _EXTRAS_MOFUNC_REF
#else
	#define _EXTRAS_MOFUNC_REF
	#define _EXTRAS_MOFUNC_INV_QUALS _EXTRAS_MOFUNC_CV&
#endif

#define _EXTRAC_MOFUNC_CV_REF _EXTRAS_MOFUNC_CV _EXTRAS_MOFUNC_REF

namespace extras
{

template<typename Res, typename... FnArgs, bool NX>
class move_only_function<Res(FnArgs...) _EXTRAC_MOFUNC_CV_REF noexcept(NX)> : detail::move_only_function_base {
	using base_type = detail::move_only_function_base;

	template<typename T>
	using callable_trait_t
		= std::conditional_t<NX, std::is_nothrow_invocable_r<Res, T, FnArgs...>, std::is_invocable_r<Res, T, FnArgs...>>;

	template<typename T>
	constexpr static bool is_callable_from_v
		= std::conjunction_v<callable_trait_t<T _EXTRAC_MOFUNC_CV_REF>, callable_trait_t<T _EXTRAS_MOFUNC_INV_QUALS>>;

	template<typename>
	constexpr static bool is_in_place_type_v = false;

	template<typename T>
	constexpr static bool is_in_place_type_v<std::in_place_type_t<T>> = true;

public:
	using result_type = Res;

	move_only_function() noexcept = default;

	move_only_function(std::nullptr_t) noexcept {}

	move_only_function(move_only_function&& other) noexcept
		: base_type(static_cast<base_type&&>(other)), m_invoke(std::exchange(other.m_invoke, nullptr))
	{}

	move_only_function(const move_only_function&) = delete;

	template<typename Fn, typename Vt = std::decay_t<Fn>>
		requires(!std::is_same_v<Vt, move_only_function>) && (!is_in_place_type_v<Vt>) && is_callable_from_v<Vt>
	move_only_function(Fn&& fn) noexcept(is_init_nothrow<Vt, Fn>())
	{
		if constexpr (std::is_function_v<std::remove_pointer_t<Vt>> || std::is_member_pointer_v<Vt>
			|| detail::is_move_only_function_v<Vt>) {
			if (fn == nullptr) {
				return;
			}
		}
		init<Vt>(std::forward<Fn>(fn));
		m_invoke = &invoke_fn<Vt>;
	}

	template<typename T, typename... Args>
		requires std::is_constructible_v<T, Args...> && is_callable_from_v<T>
	explicit move_only_function(std::in_place_type_t<T>, Args&&... args) noexcept(is_init_nothrow<T, Args...>())
		: m_invoke(&invoke_fn<T>)
	{
		static_assert(std::is_same_v<std::decay_t<T>, T>);
		init<T>(std::forward<Args>(args)...);
	}

	template<typename T, typename U, typename... Args>
		requires std::is_constructible_v<T, std::initializer_list<U>&, Args...> && is_callable_from_v<T>
	explicit move_only_function(std::in_place_type_t<T>, std::initializer_list<U> ilist, Args&&... args) noexcept(
		is_init_nothrow<T, std::initializer_list<U>&, Args...>())
		: m_invoke(&invoke_fn<T>)
	{
		static_assert(std::is_same_v<std::decay_t<T>, T>);
		init<T>(ilist, std::forward<Args>(args)...);
	}

	move_only_function& operator=(move_only_function&& other) noexcept
	{
		base_type::operator=(static_cast<base_type&&>(other));
		m_invoke = std::exchange(other.m_invoke, nullptr);
		return *this;
	}

	move_only_function& operator=(const move_only_function&) = delete;

	move_only_function& operator=(std::nullptr_t) noexcept
	{
		base_type::operator=(nullptr);
		m_invoke = nullptr;
		return *this;
	}

	template<typename Fn>
		requires std::is_constructible_v<move_only_function, Fn>
	move_only_function& operator=(Fn&& fn) noexcept(std::is_nothrow_constructible_v<move_only_function, Fn>)
	{
		move_only_function(std::forward<Fn>(fn)).swap(*this);
		return *this;
	}

	~move_only_function() = default;

	explicit operator bool() const noexcept { return m_invoke != nullptr; }

	Res operator()(FnArgs... args) _EXTRAC_MOFUNC_CV_REF noexcept(NX)
	{
		assert(*this != nullptr);
		return m_invoke(this, std::forward<FnArgs>(args)...);
	}

	void swap(move_only_function& other) noexcept
	{
		base_type::swap(other);
		std::swap(m_invoke, other.m_invoke);
	}

	friend void swap(move_only_function& a, move_only_function& b) noexcept { a.swap(b); }

	friend bool operator==(const move_only_function& x, std::nullptr_t) noexcept { return x.m_invoke == nullptr; }

private:
	template<typename T>
	using param_t = std::conditional_t<std::is_scalar_v<T>, T, T&&>;

	using invoker_t = Res (*)(base_type _EXTRAS_MOFUNC_CV*, param_t<FnArgs>...) noexcept(NX);

	template<typename T>
	static Res invoke_fn(base_type _EXTRAS_MOFUNC_CV* self, param_t<FnArgs>... args) noexcept(NX)
	{
		using TCv = T _EXTRAS_MOFUNC_CV;
		using TInv = T _EXTRAS_MOFUNC_INV_QUALS;

		// Have no `std::invoke_r<Res>` before C++23
		if constexpr (std::is_void_v<Res>) {
			std::invoke(std::forward<TInv>(*access<TCv>(self)), std::forward<param_t<FnArgs>>(args)...);
		} else {
			return std::invoke(std::forward<TInv>(*access<TCv>(self)), std::forward<param_t<FnArgs>>(args)...);
		}
	}

	invoker_t m_invoke = nullptr;
};

#undef _EXTRAS_MOFUNC_CV_REF
#undef _EXTRAS_MOFUNC_CV
#undef _EXTRAS_MOFUNC_REF
#undef _EXTRAS_MOFUNC_INV_QUALS

} // namespace extras
