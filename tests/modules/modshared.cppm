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

// Exported variable
MODSHARED_API extern int32_t g_calls;

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
