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
using year = ::date::year;

using value = std::variant<
	std::int32_t,      // signed TINYINT, SMALLINT, MEDIUMINT, INT
	std::int64_t,      // signed BIGINT
	std::uint32_t,     // unsigned TINYINT, SMALLINT, MEDIUMINT, INT
	std::uint64_t,     // unsigned BIGINT
	std::string_view,  // CHAR, VARCHAR, BINARY, VARBINARY, TEXT (all sizes), BLOB (all sizes), ENUM, SET, DECIMAL, BIT, GEOMTRY
	float,             // FLOAT
	double,            // DOUBLE
	date,              // DATE
	datetime,          // DATETIME, TIMESTAMP
	time,              // TIME
	year,              // YEAR
	std::nullptr_t     // Any of the above when the value is NULL
>;

}

// Range checks
namespace mysql
{
namespace detail
{

constexpr date min_date = ::date::day(1)/::date::January/::date::year(100); // some implementations support less than the official
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
