module;

#include <compare>
#include <cstdint>

// EXPERIMENT: module interface partition, re-exported by `tk.toml`
export module tk.toml:datetime;

export namespace tk::toml
{

struct LocalDate {
	int32_t year = 0;
	uint8_t month = 1;
	uint8_t day = 1;

	auto operator<=>(const LocalDate &) const = default;
};

struct LocalTime {
	uint8_t hour = 0;
	uint8_t minute = 0;
	uint8_t second = 0;
	// Fractional seconds beyond nanosecond precision are truncated
	uint32_t nanosecond = 0;

	auto operator<=>(const LocalTime &) const = default;
};

struct LocalDateTime {
	LocalDate date;
	LocalTime time;

	auto operator<=>(const LocalDateTime &) const = default;
};

struct OffsetDateTime {
	LocalDate date;
	LocalTime time;
	// Offset from UTC in minutes, zero for `Z`
	int16_t offset_minutes = 0;

	// NOTE: compares fields, not moments in time
	auto operator<=>(const OffsetDateTime &) const = default;
};

} // namespace tk::toml
