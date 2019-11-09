#ifndef INCLUDE_MYSQL_VALUE_HPP_
#define INCLUDE_MYSQL_VALUE_HPP_

#include <variant>
#include <cstdint>
#include <string_view>
#include <date/date.h>
#include <chrono>

namespace mysql
{

using date = ::date::sys_days;
using datetime = ::date::sys_time<std::chrono::microseconds>;
using time = std::chrono::microseconds;

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
 * field_type::tiny: (u)int8_t
 * field_type::short: (u)int16_t
 * field_type::year: uint16_t
 * field_type::int24: (u)int32_t
 * field_type::long_: (u)int32_t
 * field_type::longlong: (u)int64_t
 * field_type::float_: float
 * field_type::double_: double
 * field_type::timestamp: datetime
 * field_type::date: date
 * field_type::datetime: datetime
 * field_type::time: time
 * field_type::null: nullptr_t
 */
using value = std::variant<
	std::int32_t,
	std::int64_t,
	std::uint32_t,
	std::uint64_t,
	std::string_view,
	float,
	double,
	date,
	datetime,
	time,
	std::nullptr_t
>;

}

// Range checks
namespace mysql
{
namespace detail
{

constexpr date min_date = ::date::day(1)/::date::January/::date::year(1000);
constexpr date max_date = ::date::day(31)/::date::December/::date::year(9999);
constexpr datetime min_datetime = min_date;
constexpr datetime max_datetime = max_date + std::chrono::hours(24) - std::chrono::microseconds(1);
constexpr time min_time = -std::chrono::hours(839);
constexpr time max_time = std::chrono::hours(839);

static_assert(date::min() <= min_date);
static_assert(date::max() >= max_date);
static_assert(datetime::min() <= min_datetime);
static_assert(datetime::max() >= max_datetime);
static_assert(time::min() <= min_time);
static_assert(time::max() >= max_time);

}
}

#endif /* INCLUDE_MYSQL_VALUE_HPP_ */
