#ifndef TEST_TEST_COMMON_HPP_
#define TEST_TEST_COMMON_HPP_

#include "mysql/value.hpp"
#include "mysql/row.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <type_traits>
#include <ostream>

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

inline date makedate(int years, int months, int days)
{
	return mysql::date(::date::year(years)/::date::month(months)/::date::day(days));
}

inline datetime makedt(int years, int months, int days, int hours=0, int mins=0, int secs=0, int micros=0)
{
	return mysql::datetime(mysql::date(::date::year(years)/::date::month(months)/::date::day(days))) +
		   std::chrono::hours(hours) + std::chrono::minutes(mins) +
		   std::chrono::seconds(secs) + std::chrono::microseconds(micros);
}

inline mysql::time maket(int hours, int mins, int secs, int micros=0)
{
	return std::chrono::hours(hours) + std::chrono::minutes(mins)
	     + std::chrono::seconds(secs) + std::chrono::microseconds(micros);
}

template <std::size_t N>
inline std::string_view makesv(const char (&value) [N])
{
	static_assert(N>=1);
	return std::string_view(value, N-1); // discard null terminator
}

template <std::size_t N>
inline std::string_view makesv(const std::uint8_t (&value) [N])
{
	return std::string_view(reinterpret_cast<const char*>(value), N);
}

inline std::string_view makesv(const std::uint8_t* value, std::size_t size)
{
	return std::string_view(reinterpret_cast<const char*>(value), size);
}


inline void validate_string_contains(std::string value, const std::vector<std::string>& to_check)
{
	std::transform(value.begin(), value.end(), value.begin(), &tolower);
	for (const auto& elm: to_check)
	{
		EXPECT_THAT(value, testing::HasSubstr(elm));
	}
}

inline void validate_error_info(const mysql::error_info& value, const std::vector<std::string>& to_check)
{
	validate_string_contains(value.message(), to_check);
}

inline std::string buffer_diff(std::string_view s0, std::string_view s1)
{
	std::ostringstream ss;
	ss << std::hex;
	for (std::size_t i = 0; i < std::min(s0.size(), s1.size()); ++i)
	{
		unsigned b0 = reinterpret_cast<const std::uint8_t*>(s0.data())[i];
		unsigned b1 = reinterpret_cast<const std::uint8_t*>(s1.data())[i];
		if (b0 != b1)
		{
			ss << "i=" << i << ": " << b0 << " != " << b1 << "\n";
		}
	}
	if (s0.size() != s1.size())
	{
		ss << "sizes: " << s0.size() << " != " << s1.size() << "\n";
	}
	return ss.str();
}

inline void compare_buffers(std::string_view s0, std::string_view s1, const char* msg = "")
{
	EXPECT_EQ(s0, s1) << msg << ":\n" << buffer_diff(s0, s1);
}

struct named_test {};

template <typename T, typename=std::enable_if_t<std::is_base_of_v<named_test, T>>>
std::ostream& operator<<(std::ostream& os, const T& v) { return os << v.name; }

constexpr auto test_name_generator = [](const auto& param_info) {
	return param_info.param.name;
};

}
}



#endif /* TEST_TEST_COMMON_HPP_ */
