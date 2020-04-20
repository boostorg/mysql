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

inline void validate_error_info(const boost::mysql::error_info& value, const std::vector<std::string>& to_check)
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

inline void check_call(const std::string& command)
{
    int code = std::system(command.c_str());
    if (code != 0) // we are assuming 0 means success
    {
        throw std::runtime_error(
            detail::stringize("Command '", command, "' returned status code ", code)
        );
    }
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

struct named_param {};

template <typename T, typename=std::enable_if_t<std::is_base_of_v<named_param, T>>>
std::ostream& operator<<(std::ostream& os, const T& v) { return os << v.name; }

constexpr auto test_name_generator = [](const auto& param_info) {
    std::ostringstream os;
    os << param_info.param;
    std::string res = os.str();
    std::replace_if(res.begin(), res.end(), [](char c) { return !std::isalnum(c); }, '_');
    return res;
};

} // test
} // mysql
} // boost



#endif /* TEST_TEST_COMMON_HPP_ */
