// EXPERIMENT: a module exported from a shared library, depending on modules
// of other libraries (`m`, a static library wrapper, and `tk.rng`, a real module)

module;

#include <modshared/api.hpp>

#include <cstdint>
#include <string>

export module modshared;

export import m;
import tk.rng;

export namespace modshared
{

// Exported class with out-of-line, inline and virtual members
class MODSHARED_API Counter {
public:
	Counter() = default;
	// `dllexport` instantiates all special members, implicit copy ones would trigger
	// `-Wdeprecated-copy-with-user-provided-dtor` on Windows only
	Counter(const Counter &) = default;
	Counter &operator=(const Counter &) = default;
	virtual ~Counter();

	void add(int32_t value) noexcept;
	int32_t value() const noexcept { return m_value; }

	virtual std::string name() const;

private:
	int32_t m_value = 0;
};

// Exported functions taking/returning types from other modules
MODSHARED_API m::float3 scaled(const m::float3 &v, float s) noexcept;
MODSHARED_API uint64_t seededRandom(uint64_t seed) noexcept;

// Number of calls to the above functions. NOTE: not an exported variable: `dllexport` from
// the BMI applies in importers too (Clang does not turn it into `dllimport` there), so they
// reference `g_calls` directly instead of `__imp_g_calls` and fail to link on Windows.
// Functions work only thanks to import library thunks.
MODSHARED_API int32_t calls() noexcept;

// Templates and inline functions are instantiated/emitted by importers
template<typename T>
constexpr T twice(T t) noexcept
{
	return t + t;
}

inline int32_t answer() noexcept
{
	return MODSHARED_SQUARE(6) + 6;
}

} // namespace modshared
