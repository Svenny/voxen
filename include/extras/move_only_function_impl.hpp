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
template<typename R, typename... Args, bool NX>
class move_only_function<R(Args...) _EXTRAC_MOFUNC_CV_REF noexcept(NX)> : detail::move_only_function_base {
	using base_type = detail::move_only_function_base;

	template<typename T>
	using callable_trait_t
		= std::conditional_t<NX, std::is_nothrow_invocable_r<R, T, Args...>, std::is_invocable_r<R, T, Args...>>;

	template<typename T>
	constexpr static bool is_callable_from_v
		= std::conjunction_v<callable_trait_t<T _EXTRAC_MOFUNC_CV_REF>, callable_trait_t<T _EXTRAS_MOFUNC_INV_QUALS>>;

	template<typename>
	constexpr static bool is_in_place_type_v = false;

	template<typename T>
	constexpr static bool is_in_place_type_v<std::in_place_type_t<T>> = true;

public:
	using result_type = R;

	move_only_function() noexcept {}

	move_only_function(std::nullptr_t) noexcept {}

	move_only_function(move_only_function&& other) noexcept
		: base_type(static_cast<base_type&&>(other)), m_invoke(std::exchange(other.m_invoke, nullptr))
	{}

	template<typename _Fn, typename _Vt = std::decay_t<_Fn>>
		requires(!std::is_same_v<_Vt, move_only_function>) && (!is_in_place_type_v<_Vt>) && is_callable_from_v<_Vt>
	move_only_function(_Fn&& __f) noexcept(is_init_nothrow<_Vt, _Fn>())
	{
		if constexpr (std::is_function_v<std::remove_pointer_t<_Vt>> || std::is_member_pointer_v<_Vt>
			|| detail::is_move_only_function_v<_Vt>) {
			if (__f == nullptr) {
				return;
			}
		}
		init<_Vt>(std::forward<_Fn>(__f));
		m_invoke = &_S_invoke<_Vt>;
	}

	template<typename _Tp, typename... _Args>
		requires std::is_constructible_v<_Tp, _Args...> && is_callable_from_v<_Tp>
	explicit move_only_function(std::in_place_type_t<_Tp>, _Args&&... __args) noexcept(is_init_nothrow<_Tp, _Args...>())
		: m_invoke(&_S_invoke<_Tp>)
	{
		static_assert(std::is_same_v<std::decay_t<_Tp>, _Tp>);
		init<_Tp>(std::forward<_Args>(__args)...);
	}

	template<typename _Tp, typename _Up, typename... _Args>
		requires std::is_constructible_v<_Tp, std::initializer_list<_Up>&, _Args...> && is_callable_from_v<_Tp>
	explicit move_only_function(std::in_place_type_t<_Tp>, std::initializer_list<_Up> __il, _Args&&... __args) noexcept(
		is_init_nothrow<_Tp, std::initializer_list<_Up>&, _Args...>())
		: m_invoke(&_S_invoke<_Tp>)
	{
		static_assert(std::is_same_v<std::decay_t<_Tp>, _Tp>);
		init<_Tp>(__il, std::forward<_Args>(__args)...);
	}

	move_only_function& operator=(move_only_function&& __x) noexcept
	{
		base_type::operator=(static_cast<base_type&&>(__x));
		m_invoke = std::__exchange(__x.m_invoke, nullptr);
		return *this;
	}

	move_only_function& operator=(std::nullptr_t) noexcept
	{
		base_type::operator=(nullptr);
		m_invoke = nullptr;
		return *this;
	}

	template<typename _Fn>
		requires std::is_constructible_v<move_only_function, _Fn>
	move_only_function& operator=(_Fn&& __f) noexcept(std::is_nothrow_constructible_v<move_only_function, _Fn>)
	{
		move_only_function(std::forward<_Fn>(__f)).swap(*this);
		return *this;
	}

	~move_only_function() = default;

	explicit operator bool() const noexcept { return m_invoke != nullptr; }

	R operator()(Args... args) _EXTRAC_MOFUNC_CV_REF noexcept(NX)
	{
		assert(*this != nullptr);
		return m_invoke(this, std::forward<Args>(args)...);
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

	using invoker_t = R (*)(base_type _EXTRAS_MOFUNC_CV*, param_t<Args>...) noexcept(NX);

	template<typename T>
	static R _S_invoke(base_type _EXTRAS_MOFUNC_CV* self, param_t<Args>... args) noexcept(NX)
	{
		using _TpCv = T _EXTRAS_MOFUNC_CV;
		using _TpInv = T _EXTRAS_MOFUNC_INV_QUALS;
		return std::invoke(std::forward<_TpInv>(*access<_TpCv>(self)), std::forward<param_t<Args>>(args)...);
	}

	invoker_t m_invoke = nullptr;
};

#undef _EXTRAS_MOFUNC_CV_REF
#undef _EXTRAS_MOFUNC_CV
#undef _EXTRAS_MOFUNC_REF
#undef _EXTRAS_MOFUNC_INV_QUALS

} // namespace extras
