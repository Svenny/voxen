// EXPERIMENT: a module of a static library re-exporting a module of a shared library

module;

#include <cstdint>
#include <string>
#include <utility>

export module modstatic;

export import modshared;

export namespace modstatic
{

// Derives from a class exported from the shared library
class NamedCounter final : public modshared::Counter {
public:
	explicit NamedCounter(std::string name) : m_name(std::move(name)) {}

	std::string name() const override;

private:
	std::string m_name;
};

int32_t sumOfSquares(int32_t a, int32_t b) noexcept;

} // namespace modstatic

// Non-inline definitions in the interface unit itself, no implementation unit needed
namespace modstatic
{

std::string NamedCounter::name() const
{
	return m_name;
}

int32_t sumOfSquares(int32_t a, int32_t b) noexcept
{
	return a * a + b * b;
}

} // namespace modstatic
