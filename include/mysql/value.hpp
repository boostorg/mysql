#ifndef INCLUDE_MYSQL_VALUE_HPP_
#define INCLUDE_MYSQL_VALUE_HPP_

#include <variant>
#include <cstdint>
#include <string_view>

namespace mysql
{

// TODO: decide a better interface for these types
struct datetime // DATETIME, DATE, TIMESTAMP
{
	std::uint16_t year;
	std::uint8_t month;
	std::uint8_t day;
	std::uint8_t hour;
	std::uint8_t minutes;
	std::uint32_t microseconds;
};

struct time // TIME
{
	bool is_negative;
	std::uint32_t days;
	std::uint8_t hours;
	std::uint8_t minutes;
	std::uint8_t seconds;
	std::uint32_t microseconds;
};

/**
 * field_type::decimal: string_view
 * field_type::varchar: string_view
 * field_type::bit: string_view
 * field_type::newdecimal: string_view
 * field_type::enum_: string_view
 * field_type::set: string_view
 * field_type::tiny_blob: string_view
 * field_type::medium_blob: string_view
 * field_type::blob: string_view
 * field_type::var_string: string_view
 * field_type::geometry: string_view
 * field_type::string: string_view
 * field_type::tiny: uint8_t/int8_t
 * field_type::short: uint16_t/int8_t
 * field_type::year: uint16_t
 * field_type::int24: uint32_t/int8_t
 * field_type::long_: uint32_t/int8_t
 * field_type::longlong: uint64_t/int8_t
 * field_type::float_: float
 * field_type::double_: double
 * field_type::timestamp: datetime
 * field_type::date: datetime
 * field_type::datetime: datetime
 * field_type::time: time
 * field_type::null: nullptr_t
 */
using value = std::variant<
	std::int8_t,
	std::int16_t,
	std::int32_t,
	std::int64_t,
	std::uint8_t,
	std::uint16_t,
	std::uint32_t,
	std::uint64_t,
	std::string_view,
	float,
	double,
	datetime,
	time,
	std::nullptr_t
>;

}



#endif /* INCLUDE_MYSQL_VALUE_HPP_ */
