#ifndef INCLUDE_MYSQL_IMPL_VALUE_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_VALUE_IMPL_HPP_

#include "mysql/impl/container_equals.hpp"

namespace mysql
{
namespace detail
{

// Max/min
constexpr date min_date = ::date::day(1)/::date::January/::date::year(100); // some implementations support less than the official
constexpr date max_date = ::date::day(31)/::date::December/::date::year(9999);
constexpr datetime min_datetime = min_date;
constexpr datetime max_datetime = max_date + std::chrono::hours(24) - std::chrono::microseconds(1);
constexpr time min_time = -std::chrono::hours(839);
constexpr time max_time = std::chrono::hours(839);

// Range checks
static_assert(date::min() <= min_date);
static_assert(date::max() >= max_date);
static_assert(datetime::min() <= min_datetime);
static_assert(datetime::max() >= max_datetime);
static_assert(time::min() <= min_time);
static_assert(time::max() >= max_time);

}
}

inline bool mysql::operator==(
	const value& lhs,
	const value& rhs
)
{
	if (lhs.index() != rhs.index()) return false;
	return std::visit([&lhs](const auto& rhs_value) {
		using T = std::decay_t<decltype(rhs_value)>;
		return std::get<T>(lhs) == rhs_value;
	}, rhs);
}

inline bool mysql::operator==(
	const std::vector<value>& lhs,
	const std::vector<value>& rhs
)
{
	return detail::container_equals(lhs, rhs);
}


#endif /* INCLUDE_MYSQL_IMPL_VALUE_IMPL_HPP_ */
