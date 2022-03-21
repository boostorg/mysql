//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_NETWORK_RESULT_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_NETWORK_RESULT_HPP

#include <boost/optional/optional.hpp>
#include <boost/mysql/error.hpp>
#include <vector>
#include <string>


namespace boost {
namespace mysql {
namespace test {

struct no_result {};

struct network_result_base
{
    error_code err;
    boost::optional<error_info> info; // some network_function's don't provide this

    network_result_base() = default;
    network_result_base(error_code ec) : err(ec) {}
    network_result_base(error_code ec, error_info&& info): err(ec), info(std::move(info)) {}

    void validate_no_error() const;

    // Use when you don't care or can't determine the kind of error
    void validate_any_error(const std::vector<std::string>& expected_msg={}) const;

    void validate_error(
        error_code expected_errc,
        const std::vector<std::string>& expected_msg
    ) const;

    void validate_error(
        errc expected_errc,
        const std::vector<std::string>& expected_msg
    ) const
    {
        validate_error(make_error_code(expected_errc), expected_msg);
    }
};

template <class T>
struct network_result : network_result_base
{
    T value;

    network_result() = default;
    network_result(error_code ec, error_info&& info, T&& value = {}):
        network_result_base(ec, std::move(info)), value(std::move(value)) {}
    network_result(error_code ec, T&& value = {}):
        network_result_base(ec), value(std::move(value)) {}

    T get() &&
    {
        validate_no_error();
        return std::move(value);
    }
};

} // test
} // mysql
} // boost


#endif
