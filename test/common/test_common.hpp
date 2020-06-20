//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_COMMON_HPP
#define BOOST_MYSQL_TEST_COMMON_COMMON_HPP

#include "boost/mysql/value.hpp"
#include "boost/mysql/row.hpp"
#include "boost/mysql/error.hpp"
#include "boost/mysql/connection_params.hpp"
#include "boost/mysql/detail/auxiliar/stringize.hpp"
#include "boost/mysql/detail/protocol/date.hpp"
#include <boost/asio/buffer.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <type_traits>
#include <ostream>
#include <cassert>
#include <stdexcept>

namespace boost {
namespace mysql {
namespace test {

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

inline date makedate(int num_years, unsigned num_months, unsigned num_days)
{
    return date(days(detail::ymd_to_days(
        detail::year_month_day{num_years, num_months, num_days})));
}

inline datetime makedt(int years, unsigned months, unsigned days, int hours=0, int mins=0, int secs=0, int micros=0)
{
    return datetime(makedate(years, months, days)) +
           std::chrono::hours(hours) + std::chrono::minutes(mins) +
           std::chrono::seconds(secs) + std::chrono::microseconds(micros);
}

inline mysql::time maket(int hours, int mins, int secs, int micros=0)
{
    return std::chrono::hours(hours) + std::chrono::minutes(mins)
         + std::chrono::seconds(secs) + std::chrono::microseconds(micros);
}

template <std::size_t N>
inline boost::string_view makesv(const char (&value) [N])
{
    return detail::make_string_view(value);
}

template <std::size_t N>
inline boost::string_view makesv(const std::uint8_t (&value) [N])
{
    return boost::string_view(reinterpret_cast<const char*>(value), N);
}

inline boost::string_view makesv(const std::uint8_t* value, std::size_t size)
{
    return boost::string_view(reinterpret_cast<const char*>(value), size);
}


inline void validate_string_contains(std::string value, const std::vector<std::string>& to_check)
{
    std::transform(value.begin(), value.end(), value.begin(), &tolower);
    for (const auto& elm: to_check)
    {
        EXPECT_THAT(value, testing::HasSubstr(elm));
    }
}

inline void validate_error_info(const boost::mysql::error_info& value, const std::vector<std::string>& to_check)
{
    validate_string_contains(value.message(), to_check);
}

inline std::string buffer_diff(boost::string_view s0, boost::string_view s1)
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

inline void compare_buffers(boost::string_view s0, boost::string_view s1, const char* msg = "")
{
    EXPECT_EQ(s0, s1) << msg << ":\n" << buffer_diff(s0, s1);
}

inline void concat(std::vector<std::uint8_t>& lhs, boost::asio::const_buffer rhs)
{
    auto current_size = lhs.size();
    lhs.resize(current_size + rhs.size());
    memcpy(lhs.data() + current_size, rhs.data(), rhs.size());
}

inline void concat(std::vector<std::uint8_t>& lhs, const std::vector<uint8_t>& rhs)
{
    concat(lhs, boost::asio::buffer(rhs));
}

inline std::vector<std::uint8_t> concat_copy(
    std::vector<uint8_t>&& lhs,
    const std::vector<uint8_t>& rhs
)
{
    concat(lhs, rhs);
    return std::move(lhs);
}

inline const char* to_string(ssl_mode m)
{
    switch (m)
    {
    case ssl_mode::disable: return "ssldisable";
    case ssl_mode::enable: return "sslenable";
    case ssl_mode::require: return "sslrequire";
    default: assert(false); return "";
    }
}

// All the param types used in our parametric tests will have
// a name() member function convertible to std::string. GTest needs
// the parameter to be streamable; we tag our types with this named_tag
// so the operator<< is found by ADL.
struct named_tag {};

class named : public named_tag
{
    std::string name_;
public:
    named(std::string&& n): name_(std::move(n)) {}
    const std::string& name() const noexcept { return name_; }
};

template <
    typename T,
    typename=typename std::enable_if<
        std::is_base_of<named_tag, T>::value
    >::type
>
std::ostream& operator<<(std::ostream& os, const T& v) { return os << v.name(); }

struct test_name_generator_t
{
    template <typename T>
    std::string operator()(const testing::TestParamInfo<T>& param_info) const
    {
        return param_info.param.name();
    }
};

constexpr test_name_generator_t test_name_generator;

} // test
} // mysql
} // boost



#endif /* TEST_TEST_COMMON_HPP_ */
