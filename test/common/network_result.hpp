//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_NETWORK_RESULT_HPP
#define BOOST_MYSQL_TEST_COMMON_NETWORK_RESULT_HPP

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/server_diagnostics.hpp>

#include <boost/optional/optional.hpp>
#include <boost/test/tools/context.hpp>

#include <string>
#include <vector>

#include "test_common.hpp"

namespace boost {
namespace mysql {
namespace test {

struct no_result
{
};

struct network_result_base
{
    error_code err;
    boost::optional<server_diagnostics> diag;  // some network_function's don't provide this

    network_result_base() = default;
    network_result_base(error_code ec) : err(ec) {}
    network_result_base(error_code ec, server_diagnostics&& diag) : err(ec), diag(std::move(diag))
    {
    }

    void validate_no_error() const
    {
        BOOST_TEST_CONTEXT(
            "server_diagnostics= " << server_diag_c_str() << ", error_code=" << err.message()
        )
        {
            BOOST_TEST_REQUIRE(err == error_code());
            if (diag)
            {
                BOOST_TEST(diag->message() == "");
            }
        }
    }

    // Use when you don't care or can't determine the kind of error
    void validate_any_error(const std::vector<std::string>& expected_msg = {}) const
    {
        BOOST_TEST_CONTEXT(
            "server_diagnostics= " << server_diag_c_str() << ", error_code=" << err.message()
        )
        {
            BOOST_TEST_REQUIRE(err != error_code());
            if (diag)
            {
                validate_string_contains(diag->message(), expected_msg);
            }
        }
    }

    void validate_error(error_code expected_errc, const std::vector<std::string>& expected_msg)
        const
    {
        BOOST_TEST_CONTEXT(
            "server_diagnostics= " << server_diag_c_str() << ", error_code=" << err.message()
        )
        {
            BOOST_TEST_REQUIRE(err == expected_errc);
            if (diag)
            {
                validate_string_contains(diag->message(), expected_msg);
            }
        }
    }

    void validate_error_exact(error_code expected_err, const char* expected_msg = "")
    {
        BOOST_TEST_CONTEXT(
            "server_diagnostics= " << server_diag_c_str() << ", error_code=" << err.message()
        )
        {
            BOOST_TEST_REQUIRE(err == expected_err);
            if (diag)
            {
                BOOST_TEST(diag->message() == expected_msg);
            }
        }
    }

private:
    const char* server_diag_c_str() const noexcept
    {
        return diag ? diag->message().c_str() : "<unavailable>";
    }
};

template <class T>
struct network_result : network_result_base
{
    using value_type = typename std::conditional<std::is_same<T, void>::value, no_result, T>::type;
    value_type value;

    network_result() = default;
    network_result(error_code ec, server_diagnostics&& info, value_type&& value = {})
        : network_result_base(ec, std::move(info)), value(std::move(value))
    {
    }
    network_result(error_code ec, value_type&& value = {})
        : network_result_base(ec), value(std::move(value))
    {
    }

    value_type get() &&
    {
        validate_no_error();
        return std::move(value);
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
