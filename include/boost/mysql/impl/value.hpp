#ifndef MYSQL_ASIO_IMPL_VALUE_HPP
#define MYSQL_ASIO_IMPL_VALUE_HPP

#include "boost/mysql/detail/aux/container_equals.hpp"

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

struct print_visitor
{
	std::ostream& os;

	print_visitor(std::ostream& os): os(os) {};

	template <typename T>
	void operator()(const T& value) const { os << value; }

	void operator()(const date& value) const { ::date::operator<<(os, value); }
	void operator()(const time& value) const
	{
		char buffer [100] {};
		const char* sign = value < std::chrono::microseconds(0) ? "-" : "";
		auto hours = std::abs(std::chrono::duration_cast<std::chrono::hours>(value).count());
		auto mins = std::abs(std::chrono::duration_cast<std::chrono::minutes>(value % std::chrono::hours(1)).count());
		auto secs = std::abs(std::chrono::duration_cast<std::chrono::seconds>(value % std::chrono::minutes(1)).count());
		auto micros = std::abs((value % std::chrono::seconds(1)).count());

		snprintf(buffer, sizeof(buffer), "%s%02d:%02u:%02u:%06u",
			sign,
			static_cast<int>(hours),
			static_cast<unsigned>(mins),
			static_cast<unsigned>(secs),
			static_cast<unsigned>(micros)
		);

		os << buffer;
	}
	void operator()(const datetime& value) const { ::date::operator<<(os, value); }
	void operator()(std::nullptr_t) const { os << "<NULL>"; }
};

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

inline std::ostream& mysql::operator<<(
	std::ostream& os,
	const value& value
)
{
	std::visit(detail::print_visitor(os), value);
	return os;
}

template <typename... Types>
std::array<mysql::value, sizeof...(Types)> mysql::make_values(
	Types&&... args
)
{
	return std::array<mysql::value, sizeof...(Types)>{value(std::forward<Types>(args))...};
}


#endif
