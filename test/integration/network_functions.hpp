//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP

#include "boost/mysql/connection.hpp"
#include "test_common.hpp"
#include <gtest/gtest.h>
#include <forward_list>
#include <optional>

/**
 * A mechanism to test all variants of a network algorithm (e.g. synchronous
 * with exceptions, asynchronous with coroutines...) without writing
 * the same test several times.
 *
 * All network algorithm variants are transformed to a single one: a synchronous
 * one, returning a network_result<T>. A network_result<T> contains a T, an error_code
 * and an error_info. network_functions is an interface, which each variant implements.
 * Instead of directly calling connection, prepared_statement and resultset network
 * functions directly, tests use the network_functions interface. Tests are then
 * parameterized (e.g. TEST_P) and run over all possible implementations of
 * network_functions.
 *
 * To make things more interesting, network_functions interface is also a template,
 * allowing tests to be run over different stream types (e.g. TCP, UNIX sockets...).
 * Use BOOST_MYSQL_NETWORK_TEST* macros and NetworkTest<Stream> to achieve this.
 *
 * The following variants are currently supported:
 *  - Synchronous with error codes.
 *  - Synchronous with exceptions.
 *  - Asynchronous, with callbacks, with error_info.
 *  - Asynchronous, with callbacks, without error_info.
 *  - Asynchronous, with coroutines, with error_info.
 *  - Asynchronous, with coroutines, without error_info.
 *  - Asynchronous, with futures.
 */

namespace boost {
namespace mysql {
namespace test {

struct no_result {};

template <typename T>
struct network_result
{
    error_code err;
    std::optional<error_info> info; // some async initiators (futures) don't support this
    T value;

    network_result() = default;

    network_result(error_code ec, error_info info, T&& value = {}):
        err(ec), info(std::move(info)), value(std::move(value)) {}

    network_result(error_code ec, T&& value = {}):
        err(ec), value(std::move(value)) {}

    void validate_no_error() const
    {
        ASSERT_EQ(err, error_code()) << "with error_info= " <<
                (info ? info->message().c_str() : "<unavailable>");
        if (info)
        {
            EXPECT_EQ(*info, error_info());
        }
    }

    // Use when you don't care or can't determine the kind of error
    void validate_any_error(
        const std::vector<std::string>& expected_msg={}
    ) const
    {
        ASSERT_NE(err, error_code()) << "with error_info= " <<
                (info ? info->message().c_str() : "<unavailable>");
        if (info)
        {
            validate_string_contains(info->message(), expected_msg);
        }
    }

    void validate_error(
        error_code expected_errc,
        const std::vector<std::string>& expected_msg
    ) const
    {
        EXPECT_EQ(err, expected_errc);
        if (info)
        {
            validate_string_contains(info->message(), expected_msg);
        }
    }

    void validate_error(
        errc expected_errc,
        const std::vector<std::string>& expected_msg
    ) const
    {
        validate_error(detail::make_error_code(expected_errc), expected_msg);
    }
};

using value_list_it = std::forward_list<value>::const_iterator;

template <typename Stream>
class network_functions
{
public:
    using connection_type = socket_connection<Stream>;
    using prepared_statement_type = prepared_statement<Stream>;
    using resultset_type = resultset<Stream>;

    virtual ~network_functions() = default;
    virtual const char* name() const = 0;
    virtual network_result<no_result> connect(connection_type&, const typename Stream::endpoint_type&,
            const connection_params&) = 0;
    virtual network_result<no_result> handshake(connection_type&, const connection_params&) = 0;
    virtual network_result<resultset_type> query(connection_type&, std::string_view query) = 0;
    virtual network_result<prepared_statement_type> prepare_statement(
            connection_type&, std::string_view statement) = 0;
    virtual network_result<resultset_type> execute_statement(
            prepared_statement_type&, value_list_it params_first, value_list_it params_last) = 0;
    virtual network_result<resultset_type> execute_statement(
            prepared_statement_type&, const std::vector<value>&) = 0;
    virtual network_result<no_result> close_statement(prepared_statement_type&) = 0;
    virtual network_result<const row*> fetch_one(resultset_type&) = 0;
    virtual network_result<std::vector<owning_row>> fetch_many(resultset_type&, std::size_t count) = 0;
    virtual network_result<std::vector<owning_row>> fetch_all(resultset_type&) = 0;
    virtual network_result<no_result> quit(connection_type&) = 0;
    virtual network_result<no_result> close(connection_type&) = 0;
};

template <typename Stream>
using network_function_array = std::array<network_functions<Stream>*, 7>;

template <typename Stream>
network_function_array<Stream> make_all_network_functions();

} // test
} // mysql
} // boost



#endif /* TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP_ */
