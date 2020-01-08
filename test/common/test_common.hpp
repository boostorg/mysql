#ifndef TEST_TEST_COMMON_HPP_
#define TEST_TEST_COMMON_HPP_

#include "mysql/value.hpp"
#include "mysql/row.hpp"
#include <vector>

namespace mysql
{
namespace test
{

template <typename... Types>
std::vector<value> makevalues(Types&&... args)
{
	return std::vector<value>{mysql::value(std::forward<Types>(args))...};
}

template <typename... Types>
row makerow(Types&&... args)
{
	return row(makevalues(std::forward<Types>(args)...));
}

template <typename... Types>
std::vector<row> makerows(std::size_t row_size, Types&&... args)
{
	auto values = makevalues(std::forward<Types>(args)...);
	assert(values.size() % row_size == 0);
	std::vector<row> res;
	for (std::size_t i = 0; i < values.size(); i += row_size)
	{
		std::vector<value> row_values (values.begin() + i, values.begin() + i + row_size);
		res.push_back(row(std::move(row_values)));
	}
	return res;
}

inline datetime makedt(int years, int months, int days, int hours=0, int mins=0, int secs=0, int micros=0)
{
	return mysql::date(::date::year(years)/months/days) +
		   std::chrono::hours(hours) + std::chrono::minutes(mins) +
		   std::chrono::seconds(secs) + std::chrono::microseconds(micros);
}

inline mysql::time maket(int hours, int mins, int secs, int micros=0)
{
	return std::chrono::hours(hours) + std::chrono::minutes(mins)
	     + std::chrono::seconds(secs) + std::chrono::microseconds(micros);
}

}
}



#endif /* TEST_TEST_COMMON_HPP_ */
