//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/tcp.hpp>

#include <boost/mysql/detail/config.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/assert/source_location.hpp>
#include <boost/core/span.hpp>
#include <boost/mp11/detail/mp_list.hpp>
#include <boost/optional/optional.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/tools/context.hpp>
#include <boost/test/tools/detail/per_element_manip.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <cstddef>
#include <vector>

#include "test_common/create_basic.hpp"
#include "test_common/netfun_maker.hpp"
#include "test_common/printing.hpp"
#include "test_integration/any_connection_fixture.hpp"
#include "test_integration/common.hpp"
#include "test_integration/er_connection.hpp"
#include "test_integration/network_samples.hpp"
#include "test_integration/spotchecks_helpers.hpp"
#include "test_integration/static_rows.hpp"
#include "test_integration/tcp_network_fixture.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
namespace asio = boost::asio;
using boost::test_tools::per_element;

// These tests aim to cover the 4 overloads we have for each network function,
// both for the old templated connection and for any_connection.
// A success and an error case is included for each function.

namespace {

BOOST_AUTO_TEST_SUITE(test_spotchecks)

// Map from a connection to its network functions type
template <class Conn>
struct network_functions_type;

template <>
struct network_functions_type<tcp_connection>
{
    using type = network_functions_connection;
};

template <>
struct network_functions_type<any_connection>
{
    using type = network_functions_any;
};

template <class Conn>
using network_functions_t = typename network_functions_type<Conn>::type;

template <class Conn>
boost::span<const typename network_functions_type<Conn>::type> get_network_functions()
{
    static auto res = network_functions_type<Conn>::type::all();
    return res;
}

// Fixtures
template <class Conn>
struct fixture_type;

template <>
struct fixture_type<tcp_connection>
{
    using type = tcp_network_fixture;
};

template <>
struct fixture_type<any_connection>
{
    using type = any_connection_fixture;
};

template <class Conn>
using fixture_type_t = typename fixture_type<Conn>::type;

// Connect with multi-queries enabled in a generic way
// TODO: clangd reports that these are not used (incorrectly)
// TODO: fixture connect shouldn't be used in this context
static void connect_with_multi_queries(tcp_network_fixture& fix)
{
    fix.params.set_multi_queries(true);
    fix.connect();
}

static void connect_with_multi_queries(
    any_connection_fixture& fix,
    boost::source_location loc = BOOST_CURRENT_LOCATION
)
{
    fix.connect(connect_params_builder().ssl(ssl_mode::disable).multi_queries(true).build(), loc);
}

// Simplify defining test cases involving both connection types
// The resulting test case gets the fixture (fix) and the network functions (fn) as args
#define BOOST_MYSQL_SPOTCHECK_TEST(name)                                                                \
    template <class Conn>                                                                               \
    void do_##name(fixture_type_t<Conn>& fix, const network_functions_t<Conn>& fn);                     \
    BOOST_DATA_TEST_CASE_F(tcp_network_fixture, name##_connection, network_functions_connection::all()) \
    {                                                                                                   \
        do_##name<tcp_connection>(*this, sample);                                                       \
    }                                                                                                   \
    BOOST_DATA_TEST_CASE_F(any_connection_fixture, name##_any, network_functions_any::all())            \
    {                                                                                                   \
        do_##name<any_connection>(*this, sample);                                                       \
    }                                                                                                   \
    template <class Conn>                                                                               \
    void do_##name(fixture_type_t<Conn>& fix, const network_functions_t<Conn>& fn)

auto err_samples = network_samples({
    "tcp_sync_errc",
    "tcp_sync_exc",
    "tcp_async_callback",
    "tcp_async_coroutines",
});

auto samples_with_handshake = network_samples::all_with_handshake();
auto all_samples = network_samples::all();

// prepare statement
BOOST_MYSQL_SPOTCHECK_TEST(prepare_statement_success)
{
    // Setup
    fix.connect();

    // Call the function
    statement stmt = fn.prepare_statement(fix.conn, "SELECT * FROM empty_table WHERE id IN (?, ?)").get();

    // Validate the result
    BOOST_TEST_REQUIRE(stmt.valid());
    BOOST_TEST(stmt.id() > 0u);
    BOOST_TEST(stmt.num_params() == 2u);

    // It can be executed
    results result;
    fn.execute_statement(fix.conn, stmt.bind(10, 20), result).validate_no_error();
    BOOST_TEST(result.rows().empty());
}

BOOST_MYSQL_SPOTCHECK_TEST(prepare_statement_error)
{
    // Setup
    fix.connect();

    // Call the function
    fn.prepare_statement(fix.conn, "SELECT * FROM bad_table WHERE id IN (?, ?)")
        .validate_error(common_server_errc::er_no_such_table, {"table", "doesn't exist", "bad_table"});
}

// execute
BOOST_MYSQL_SPOTCHECK_TEST(execute_success)
{
    // Setup
    fix.connect();

    // Call the function
    results result;
    fn.execute_query(fix.conn, "SELECT 'hello', 42", result).validate_no_error();

    // Check results
    BOOST_TEST(result.rows().size() == 1u);
    BOOST_TEST(result.rows()[0] == makerow("hello", 42), per_element());
    BOOST_TEST(result.meta().size() == 2u);
}

BOOST_MYSQL_SPOTCHECK_TEST(execute_error)
{
    // Setup
    fix.connect();

    // Call the function
    results result;
    fn.execute_query(fix.conn, "SELECT field_varchar, field_bad FROM one_row_table", result)
        .validate_error(common_server_errc::er_bad_field_error, {"unknown column", "field_bad"});
}

// Start execution
BOOST_MYSQL_SPOTCHECK_TEST(start_execution_success)
{
    // Setup
    fix.connect();

    // Call the function
    execution_state st;
    fn.start_execution(fix.conn, "SELECT * FROM empty_table", st).validate_no_error();

    // Check results
    BOOST_TEST(st.should_read_rows());
    validate_2fields_meta(st.meta(), "empty_table");
}

BOOST_MYSQL_SPOTCHECK_TEST(start_execution_error)
{
    // Setup
    fix.connect();

    // Call the function
    execution_state st;
    fn.start_execution(fix.conn, "SELECT field_varchar, field_bad FROM one_row_table", st)
        .validate_error(common_server_errc::er_bad_field_error, {"unknown column", "field_bad"});
}

// Close statement
BOOST_MYSQL_SPOTCHECK_TEST(close_statement_success)
{
    // Setup
    fix.connect();

    // Prepare a statement
    statement stmt = fn.prepare_statement(fix.conn, "SELECT * FROM empty_table WHERE id IN (?, ?)").get();

    // Close the statement
    fn.close_statement(fix.conn, stmt).validate_no_error();

    // The statement is no longer valid
    results result;
    fn.execute_statement(fix.conn, stmt.bind(1, 2), result).validate_any_error();
}

BOOST_MYSQL_SPOTCHECK_TEST(close_statement_error)
{
    // Setup
    fix.connect();

    // Prepare a statement
    statement stmt = fn.prepare_statement(fix.conn, "SELECT * FROM empty_table WHERE id IN (?, ?)").get();

    // Close the connection
    fn.close(fix.conn).validate_no_error();

    // Close the statement fails, as it requires communication with the server
    fn.close_statement(fix.conn, stmt).validate_any_error();
}

// Read resultset head
BOOST_MYSQL_SPOTCHECK_TEST(read_resultset_head_success)
{
    // Setup
    connect_with_multi_queries(fix);

    // Generate an execution state
    execution_state st;
    fn.start_execution(fix.conn, "SELECT 4.2e0; SELECT 'abc', 42", st).validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_rows());

    // Read the 1st resultset
    auto rws = fn.read_some_rows(fix.conn, st).get();
    BOOST_TEST_REQUIRE(st.should_read_head());
    BOOST_TEST(st.meta()[0].type() == column_type::double_);
    BOOST_TEST(rws == makerows(1, 4.2), per_element());

    // Read head
    fn.read_resultset_head(fix.conn, st).validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_rows());
    BOOST_TEST(st.meta()[0].type() == column_type::varchar);
    BOOST_TEST(st.meta()[1].type() == column_type::bigint);

    // Reading head again does nothing
    fn.read_resultset_head(fix.conn, st).validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_rows());
    BOOST_TEST(st.meta()[0].type() == column_type::varchar);
    BOOST_TEST(st.meta()[1].type() == column_type::bigint);

    // We can read rows now
    auto rows = fn.read_some_rows(fix.conn, st).get();
    BOOST_TEST((rows == makerows(2, "abc", 42)));
}

BOOST_MYSQL_SPOTCHECK_TEST(read_resultset_head_error)
{
    // Setup
    connect_with_multi_queries(fix);

    // Generate an execution state
    execution_state st;
    fn.start_execution(fix.conn, "SELECT * FROM empty_table; SELECT bad_field FROM one_row_table", st)
        .validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_rows());

    // Read the OK packet to finish 1st resultset
    fn.read_some_rows(fix.conn, st).validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_head());

    // Read head for the 2nd resultset. This one contains an error, which is detected when reading head.
    fn.read_resultset_head(fix.conn, st)
        .validate_error_exact(
            common_server_errc::er_bad_field_error,
            "Unknown column 'bad_field' in 'field list'"
        );
}

// Read some rows. No error spotcheck here
// TODO: spotcheck this as a unit test
BOOST_MYSQL_SPOTCHECK_TEST(read_some_rows_success)
{
    // Setup
    fix.connect();

    // Generate an execution state
    execution_state st;
    fn.start_execution(fix.conn, "SELECT * FROM one_row_table", st);
    BOOST_TEST_REQUIRE(st.should_read_rows());

    // Read once. st may or may not be complete, depending
    // on how the buffer reallocated memory
    auto rows = fn.read_some_rows(fix.conn, st).get();
    BOOST_TEST((rows == makerows(2, 1, "f0")));

    // Reading again should complete st
    rows = fn.read_some_rows(fix.conn, st).get();
    BOOST_TEST(rows.empty());
    validate_eof(st);

    // Reading again does nothing
    rows = fn.read_some_rows(fix.conn, st).get();
    BOOST_TEST(rows.empty());
    validate_eof(st);
}

// Ping
BOOST_MYSQL_SPOTCHECK_TEST(ping_success)
{
    // Setup
    fix.connect();

    // Success
    fn.ping(fix.conn).validate_no_error();
}

// TODO: writing to an unconnected any_connection triggers an assert right now
// This should be solved by the refactor we're delivering in
// https://github.com/boostorg/mysql/issues/230
// BOOST_MYSQL_SPOTCHECK_TEST(ping_error)
// {
//     // Pinging an unconnected connection fails
//     fn.ping(fix.conn).validate_any_error();
// }
BOOST_DATA_TEST_CASE_F(tcp_network_fixture, ping_error, network_functions_connection::all())
{
    // Ping should return an error for an unconnected connection
    sample.ping(conn).validate_any_error();
}

//
//
//
//
// Handshake
BOOST_DATA_TEST_CASE_F(network_fixture, handshake_success, samples_with_handshake)
{
    setup_and_physical_connect(sample);
    conn->handshake(params).validate_no_error();
    BOOST_TEST(conn->uses_ssl() == var->supports_ssl());
}

BOOST_DATA_TEST_CASE_F(network_fixture, handshake_error, err_samples)
{
    setup_and_physical_connect(sample);
    params.set_database("bad_database");
    conn->handshake(params).validate_error(
        common_server_errc::er_dbaccess_denied_error,
        {"database", "bad_database"}
    );
}

// Connect: success is already widely tested throughout integ tests
BOOST_DATA_TEST_CASE_F(network_fixture, connect_error, err_samples)
{
    setup(sample);
    set_credentials("integ_user", "bad_password");
    conn->connect(params).validate_error(
        common_server_errc::er_access_denied_error,
        {"access denied", "integ_user"}
    );
    BOOST_TEST(!conn->is_open());
}

// Reset connection: no server error spotcheck.
BOOST_DATA_TEST_CASE_F(network_fixture, reset_connection_success, all_samples)
{
    setup_and_connect(sample);

    // Set some variable
    results result;
    conn->execute("SET @myvar = 42", result).validate_no_error();

    // Reset connection
    conn->reset_connection().validate_no_error();

    // The variable has been reset
    conn->execute("SELECT @myvar", result);
    BOOST_TEST(result.rows().at(0).at(0).is_null());
}

// Quit connection: no server error spotcheck
BOOST_DATA_TEST_CASE_F(network_fixture, quit_success, samples_with_handshake)
{
    setup_and_connect(sample);

    // Quit
    conn->quit().validate_no_error();

    // TODO: we can't just query() and expect an error, because that's not reliable as a test.
    // We should have a flag to check whether the connection is connected or not
}

// Close connection: no server error spotcheck
// TODO: all_variants_with_handshake
BOOST_DATA_TEST_CASE_F(network_fixture, close_connection_success, samples_with_handshake)
{
    setup_and_connect(sample);

    // Close
    conn->close().validate_no_error();

    // We are no longer able to query
    boost::mysql::results result;
    conn->execute("SELECT 1", result).validate_any_error();

    // The stream is closed
    BOOST_TEST(!conn->is_open());

    // Closing again returns OK (and does nothing)
    conn->close().validate_no_error();

    // Stream (socket) still closed
    BOOST_TEST(!conn->is_open());
}

// TODO: move this to a unit test
BOOST_DATA_TEST_CASE_F(network_fixture, not_open_connection, err_samples)
{
    setup(sample);
    conn->close().validate_no_error();
    BOOST_TEST(!conn->is_open());
}

#ifdef BOOST_MYSQL_CXX14
// Execute (static) - errors are already covered by the other tests
BOOST_DATA_TEST_CASE_F(network_fixture, execute_static_success, all_samples)
{
    setup_and_connect(sample);

    er_connection::static_results_t result;
    conn->execute("CALL sp_spotchecks()", result).get();
    BOOST_TEST(result.rows<0>().size() == 1u);
    row_multifield expected{boost::optional<float>(1.1f), 11, std::string("aaa")};
    BOOST_TEST(result.rows<0>()[0] == expected);
}

// start_execution, read_resultset_head, read_some_rows success
BOOST_DATA_TEST_CASE_F(network_fixture, start_execution_and_followups_static_success, all_samples)
{
    setup_and_connect(sample);

    er_connection::static_state_t st;

    // Start
    conn->start_execution("CALL sp_spotchecks()", st).get();
    BOOST_TEST(st.should_read_rows());

    // Read r1 rows
    std::array<row_multifield, 2> storage;
    std::size_t num_rows = conn->read_some_rows(st, storage).get();
    row_multifield expected_multifield{boost::optional<float>(1.1f), 11, std::string("aaa")};  // MSVC 14.1
    BOOST_TEST(num_rows == 1u);
    BOOST_TEST(storage[0] == expected_multifield);

    // Ensure we're in the next resultset
    num_rows = conn->read_some_rows(st, storage).get();
    BOOST_TEST(num_rows == 0u);
    BOOST_TEST(st.should_read_head());

    // Read r2 head
    conn->read_resultset_head(st).get();
    BOOST_TEST(st.should_read_rows());

    // Read r2 rows
    std::array<row_2fields, 2> storage2;
    num_rows = conn->read_some_rows(st, storage2).get();
    BOOST_TEST(num_rows == 1u);
    row_2fields expected_2fields{1, std::string("f0")};
    BOOST_TEST(storage2[0] == expected_2fields);

    // Ensure we're in the next resultset
    num_rows = conn->read_some_rows(st, storage2).get();
    BOOST_TEST(num_rows == 0u);
    BOOST_TEST(st.should_read_head());

    // Read r3 head (empty)
    conn->read_resultset_head(st).get();
    BOOST_TEST(st.complete());
}

// read_some_rows failure (the other error cases are already widely tested)
BOOST_DATA_TEST_CASE_F(network_fixture, read_some_rows_error, err_samples)
{
    setup_and_connect(sample);

    er_connection::static_state_t st;

    // Start
    conn->start_execution("SELECT * FROM multifield_table WHERE id = 42", st).get();
    BOOST_TEST(st.should_read_rows());

    // No rows matched, so reading rows reads the OK packet. This will report the num resultsets mismatch
    std::array<row_multifield, 2> storage;
    conn->read_some_rows(st, storage)
        .validate_error_exact_client(boost::mysql::client_errc::num_resultsets_mismatch);
}
#endif

// set_character_set. Since this is only available in any_connection, we spotcheck this
// with netmakers and don't cover all streams
using set_charset_netmaker = netfun_maker_mem<void, any_connection, const character_set&>;

struct
{
    string_view name;
    set_charset_netmaker::signature set_character_set;
} set_charset_all_fns[] = {
    {"sync_errc",       set_charset_netmaker::sync_errc(&any_connection::set_character_set)            },
    {"sync_exc",        set_charset_netmaker::sync_exc(&any_connection::set_character_set)             },
    {"async_errinfo",   set_charset_netmaker::async_errinfo(&any_connection::async_set_character_set)  },
    {"async_noerrinfo", set_charset_netmaker::async_noerrinfo(&any_connection::async_set_character_set)},
};

BOOST_AUTO_TEST_CASE(set_character_set_success)
{
    for (const auto& fns : set_charset_all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // Setup
            boost::asio::io_context ctx;
            any_connection conn(ctx);
            conn.connect(default_connect_params());

            // Issue the command
            fns.set_character_set(conn, ascii_charset).validate_no_error();

            // Success
            BOOST_TEST(conn.current_character_set()->name == "ascii");
        }
    }
}

BOOST_AUTO_TEST_CASE(set_character_set_error)
{
    for (const auto& fns : set_charset_all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // Setup
            boost::asio::io_context ctx;
            any_connection conn(ctx);
            conn.connect(default_connect_params(ssl_mode::disable));

            // Issue the command
            fns.set_character_set(conn, character_set{"bad_charset", nullptr})
                .validate_error_exact(
                    common_server_errc::er_unknown_character_set,
                    "Unknown character set: 'bad_charset'"
                );

            // The character set was not modified
            BOOST_TEST(conn.current_character_set()->name == "utf8mb4");
        }
    }
}

// same for run_pipeline
using run_pipeline_netmaker = netfun_maker_mem<
    void,
    any_connection,
    const pipeline_request&,
    std::vector<stage_response>&>;

struct
{
    string_view name;
    run_pipeline_netmaker::signature run_pipeline;
} run_pipeline_all_fns[] = {
    {"sync_errc",       run_pipeline_netmaker::sync_errc(&any_connection::run_pipeline)            },
    {"sync_exc",        run_pipeline_netmaker::sync_exc(&any_connection::run_pipeline)             },
    {"async_errinfo",   run_pipeline_netmaker::async_errinfo(&any_connection::async_run_pipeline)  },
    {"async_noerrinfo", run_pipeline_netmaker::async_noerrinfo(&any_connection::async_run_pipeline)},
};

BOOST_AUTO_TEST_CASE(run_pipeline_success)
{
    for (const auto& fns : run_pipeline_all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // Setup
            boost::asio::io_context ctx;
            any_connection conn(ctx);
            conn.connect(default_connect_params(ssl_mode::disable));
            pipeline_request req;
            req.add_set_character_set(ascii_charset)
                .add_execute("SET @myvar = 42")
                .add_execute("SELECT @myvar");
            std::vector<stage_response> res;

            // Issue the pipeline
            fns.run_pipeline(conn, req, res).validate_no_error();

            // Success
            BOOST_TEST(conn.current_character_set().value() == ascii_charset);
            BOOST_TEST_REQUIRE(res.size() == 3u);
            BOOST_TEST(res.at(0).error() == error_code());
            BOOST_TEST(res.at(0).diag() == diagnostics());
            BOOST_TEST(res.at(1).as_results().rows().empty());
            BOOST_TEST(res.at(2).as_results().rows() == makerows(1, 42));
        }
    }
}

BOOST_AUTO_TEST_CASE(run_pipeline_error)
{
    for (const auto& fns : run_pipeline_all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // Setup
            boost::asio::io_context ctx;
            any_connection conn(ctx);
            conn.connect(default_connect_params(ssl_mode::disable));
            pipeline_request req;
            req.add_execute("SET @myvar = 42")
                .add_prepare_statement("SELECT * FROM bad_table")
                .add_execute("SELECT @myvar");
            std::vector<stage_response> res;

            // Issue the command
            fns.run_pipeline(conn, req, res)
                .validate_error_exact(
                    common_server_errc::er_no_such_table,
                    "Table 'boost_mysql_integtests.bad_table' doesn't exist"
                );

            // Stages 0 and 2 were executed successfully
            BOOST_TEST(res.size() == 3u);
            BOOST_TEST(res[0].as_results().rows().size() == 0u);
            BOOST_TEST(res[1].error() == common_server_errc::er_no_such_table);
            BOOST_TEST(
                res[1].diag() == create_server_diag("Table 'boost_mysql_integtests.bad_table' doesn't exist")
            );
            BOOST_TEST(res[2].as_results().rows() == makerows(1, 42));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()  // test_spotchecks

}  // namespace