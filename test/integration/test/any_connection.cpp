//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/engine_impl.hpp>

#include <boost/mysql/impl/internal/variant_stream.hpp>

#include <string>

#include "test_common/as_netres.hpp"
#include "test_common/create_basic.hpp"
#include "test_common/netfun_maker.hpp"
#include "test_common/printing.hpp"
#include "test_integration/any_connection_fixture.hpp"
#include "test_integration/common.hpp"

// Additional spotchecks for any_connection

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;
using boost::test_tools::per_element;

BOOST_AUTO_TEST_SUITE(test_any_connection)

// Backslash escapes
BOOST_FIXTURE_TEST_CASE(backslash_escapes, any_connection_fixture)
{
    // Backslash escapes enabled by default
    BOOST_TEST(conn.backslash_escapes());

    // Connect doesn't change the value
    conn.async_connect(connect_params_builder().disable_ssl().build(), as_netresult).validate_no_error();
    BOOST_TEST(conn.backslash_escapes());
    BOOST_TEST(conn.format_opts()->backslash_escapes);

    // Setting the SQL mode to NO_BACKSLASH_ESCAPES updates the value
    results r;
    conn.async_execute("SET sql_mode = 'NO_BACKSLASH_ESCAPES'", r, as_netresult).validate_no_error();
    BOOST_TEST(!conn.backslash_escapes());
    BOOST_TEST(!conn.format_opts()->backslash_escapes);

    // Executing a different statement doesn't change the value
    conn.async_execute("SELECT 1", r, as_netresult).validate_no_error();
    BOOST_TEST(!conn.backslash_escapes());
    BOOST_TEST(!conn.format_opts()->backslash_escapes);

    // Clearing the SQL mode updates the value
    conn.async_execute("SET sql_mode = ''", r, as_netresult).validate_no_error();
    BOOST_TEST(conn.backslash_escapes());
    BOOST_TEST(conn.format_opts()->backslash_escapes);

    // Reconnecting clears the value
    conn.async_execute("SET sql_mode = 'NO_BACKSLASH_ESCAPES'", r, as_netresult).validate_no_error();
    BOOST_TEST(!conn.backslash_escapes());
    BOOST_TEST(!conn.format_opts()->backslash_escapes);
    conn.async_connect(connect_params_builder().disable_ssl().build(), as_netresult).validate_no_error();
    BOOST_TEST(conn.backslash_escapes());
    BOOST_TEST(conn.format_opts()->backslash_escapes);
}

// Max buffer sizes
BOOST_AUTO_TEST_CASE(max_buffer_size)
{
    // Create the connection
    any_connection_params params;
    params.initial_buffer_size = 512u;
    params.max_buffer_size = 512u;
    any_connection_fixture fix(params);

    // Connect
    fix.conn.async_connect(connect_params_builder().disable_ssl().build(), as_netresult).validate_no_error();

    // Reading and writing almost 512 bytes works
    results r;
    auto q = format_sql(fix.conn.format_opts().value(), "SELECT {}", std::string(450, 'a'));
    fix.conn.async_execute(q, r, as_netresult).validate_no_error();
    BOOST_TEST(r.rows() == makerows(1, std::string(450, 'a')), per_element());

    // Trying to write more than 512 bytes fails
    q = format_sql(fix.conn.format_opts().value(), "SELECT LENGTH({})", std::string(512, 'a'));
    fix.conn.async_execute(q, r, as_netresult).validate_error(client_errc::max_buffer_size_exceeded);

    // Trying to read more than 512 bytes fails
    fix.conn.async_execute("SELECT REPEAT('a', 512)", r, as_netresult)
        .validate_error(client_errc::max_buffer_size_exceeded);
}

BOOST_FIXTURE_TEST_CASE(default_max_buffer_size_success, any_connection_fixture)
{
    // Connect
    conn.async_connect(connect_params_builder().disable_ssl().build(), as_netresult).validate_no_error();

    // Reading almost max_buffer_size works
    execution_state st;
    conn.async_start_execution("SELECT 1, REPEAT('a', 0x3f00000)", st, as_netresult).validate_no_error();
    auto rws = conn.async_read_some_rows(st, as_netresult).get();
    BOOST_TEST(rws.at(0).at(1).as_string().size() == 0x3f00000u);
}

BOOST_FIXTURE_TEST_CASE(default_max_buffer_size_error, any_connection_fixture)
{
    // Connect
    conn.async_connect(connect_params_builder().disable_ssl().build(), as_netresult).validate_no_error();

    // Trying to read more than max_buffer_size bytes fails
    results r;
    conn.async_execute("SELECT 1, REPEAT('a', 0x4000000)", r, as_netresult)
        .validate_error(client_errc::max_buffer_size_exceeded);
}

BOOST_AUTO_TEST_CASE(naggle_disabled)
{
    using netmaker_connect = netfun_maker_mem<void, any_connection, const connect_params&>;

    struct
    {
        string_view name;
        netmaker_connect::signature fn;
    } test_cases[] = {
        {"sync",  netmaker_connect::sync_errc(&any_connection::connect)          },
        {"async", netmaker_connect::async_errinfo(&any_connection::async_connect)},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Create the connection
            any_connection_fixture fix;

            // Connect
            tc.fn(fix.conn, connect_params_builder().disable_ssl().build()).validate_no_error();

            // Naggle's algorithm was disabled
            asio::ip::tcp::no_delay opt;
            static_cast<detail::engine_impl<detail::variant_stream>&>(
                detail::access::get_impl(fix.conn).get_engine()
            )
                .stream()
                .tcp_socket()
                .get_option(opt);
            BOOST_TEST(opt.value() == true);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()