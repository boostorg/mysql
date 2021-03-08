//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP

#include <boost/mysql/socket_connection.hpp>
#include "tcp_future_socket.hpp"
#include <forward_list>
#include <boost/optional/optional.hpp>

/**
 * A mechanism to test all variants of a network algorithm (e.g. synchronous
 * with exceptions, asynchronous with coroutines...) without writing
 * the same test several times.
 *
 * All network algorithm variants are transformed to a single one: a synchronous
 * one, returning a network_result<T>. A network_result<T> contains a T, an error_code
 * and an error_info. network_functions is an interface, which each variant implements.
 * Instead of calling connection, prepared_statement and resultset network
 * functions directly, tests use the network_functions interface. network_functions
 * is also a template, allowing tests to be run over different stream types.
 * network_functions is intended to be used as part of a test sample, together with
 * BOOST_MYSQL_NETWORK_TEST.
 *
 * See the implementation of all_network_functions<Stream>() for the list
 * of supported network functions.
 */

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
};

using value_list_it = std::forward_list<value>::const_iterator;

template <class Stream>
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
    virtual network_result<resultset_type> query(connection_type&, boost::string_view query) = 0;
    virtual network_result<prepared_statement_type> prepare_statement(
            connection_type&, boost::string_view statement) = 0;
    virtual network_result<resultset_type> execute_statement(
            prepared_statement_type&, const execute_params<value_list_it>& params) = 0;
    virtual network_result<resultset_type> execute_statement(
            prepared_statement_type&, const std::vector<value>&) = 0;
    virtual network_result<no_result> close_statement(prepared_statement_type&) = 0;
    virtual network_result<bool> read_one(resultset_type&, row&) = 0;
    virtual network_result<std::vector<row>> read_many(resultset_type&, std::size_t count) = 0;
    virtual network_result<std::vector<row>> read_all(resultset_type&) = 0;
    virtual network_result<no_result> quit(connection_type&) = 0;
    virtual network_result<no_result> close(connection_type&) = 0;
};

template <class Stream>
const std::vector<network_functions<Stream>*>& all_network_functions();

template <>
const std::vector<network_functions<tcp_future_socket>*>&
all_network_functions<tcp_future_socket>();

} // test
} // mysql
} // boost



#endif /* TEST_INTEGRATION_NETWORK_FUNCTIONS_HPP_ */
