//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/ssl_mode.hpp>

#include <boost/mysql/detail/config.hpp>

#include <vector>

#include "test_common/create_basic.hpp"
#include "test_common/netfun_maker.hpp"
#include "test_common/printing.hpp"
#include "test_integration/common.hpp"
#include "test_integration/er_connection.hpp"
#include "test_integration/network_test.hpp"
#include "test_integration/static_rows.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_spotchecks)

auto err_net_samples = create_network_samples({
    "tcp_sync_errc",
    "tcp_sync_exc",
    "tcp_async_callback",
    "tcp_async_coroutines",
});

// Handshake
BOOST_MYSQL_NETWORK_TEST(handshake_success, network_fixture, all_network_samples_with_handshake())
{
    setup_and_physical_connect(sample.net);
    conn->handshake(params).validate_no_error();
    BOOST_TEST(conn->uses_ssl() == var->supports_ssl());
}

BOOST_MYSQL_NETWORK_TEST(handshake_error, network_fixture, err_net_samples)
{
    setup_and_physical_connect(sample.net);
    params.set_database("bad_database");
    conn->handshake(params).validate_error(
        common_server_errc::er_dbaccess_denied_error,
        {"database", "bad_database"}
    );
}

// Connect: success is already widely tested throughout integ tests
BOOST_MYSQL_NETWORK_TEST(connect_error, network_fixture, err_net_samples)
{
    setup(sample.net);
    set_credentials("integ_user", "bad_password");
    conn->connect(params).validate_error(
        common_server_errc::er_access_denied_error,
        {"access denied", "integ_user"}
    );
    BOOST_TEST(!conn->is_open());
}

// Start execution (query)
BOOST_MYSQL_NETWORK_TEST(start_execution_query_success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);

    execution_state st;
    conn->start_execution("SELECT * FROM empty_table", st).get();
    BOOST_TEST(st.should_read_rows());
    validate_2fields_meta(st.meta(), "empty_table");
}

BOOST_MYSQL_NETWORK_TEST(start_execution_query_error, network_fixture, err_net_samples)
{
    setup_and_connect(sample.net);

    execution_state st;
    conn->start_execution("SELECT field_varchar, field_bad FROM one_row_table", st)
        .validate_error(common_server_errc::er_bad_field_error, {"unknown column", "field_bad"});
}

// execute (query)
BOOST_MYSQL_NETWORK_TEST(execute_query_success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);

    results result;
    conn->execute("SELECT 'hello', 42", result).get();
    BOOST_TEST(result.rows().size() == 1u);
    BOOST_TEST(result.rows()[0] == makerow("hello", 42));
    BOOST_TEST(result.meta().size() == 2u);
}

BOOST_MYSQL_NETWORK_TEST(execute_query_error, network_fixture, err_net_samples)
{
    setup_and_connect(sample.net);

    results result;
    conn->execute("SELECT field_varchar, field_bad FROM one_row_table", result)
        .validate_error(common_server_errc::er_bad_field_error, {"unknown column", "field_bad"});
}

// Prepare statement
BOOST_MYSQL_NETWORK_TEST(prepare_statement_success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)").get();
    BOOST_TEST_REQUIRE(stmt.valid());
    BOOST_TEST(stmt.id() > 0u);
    BOOST_TEST(stmt.num_params() == 2u);
}

BOOST_MYSQL_NETWORK_TEST(prepare_statement_error, network_fixture, err_net_samples)
{
    setup_and_connect(sample.net);
    conn->prepare_statement("SELECT * FROM bad_table WHERE id IN (?, ?)")
        .validate_error(common_server_errc::er_no_such_table, {"table", "doesn't exist", "bad_table"});
}

// Start execution (statement, iterator)
BOOST_MYSQL_NETWORK_TEST(start_execution_stmt_it_success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);

    // Prepare
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)").get();

    // Execute
    execution_state st;
    std::forward_list<field_view> stmt_params{field_view("item"), field_view(42)};
    conn->start_execution(stmt.bind(stmt_params.cbegin(), stmt_params.cend()), st).validate_no_error();
    validate_2fields_meta(st.meta(), "empty_table");
    BOOST_TEST(st.should_read_rows());
}

BOOST_MYSQL_NETWORK_TEST(start_execution_stmt_it_error, network_fixture, err_net_samples)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare
    auto stmt = conn->prepare_statement("INSERT INTO inserts_table (field_varchar, field_date) VALUES (?, ?)")
                    .get();

    // Execute
    execution_state st;
    std::forward_list<field_view> stmt_params{field_view("f0"), field_view("bad_date")};
    conn->start_execution(stmt.bind(stmt_params.cbegin(), stmt_params.cend()), st)
        .validate_error(
            common_server_errc::er_truncated_wrong_value,
            {"field_date", "bad_date", "incorrect date value"}
        );
}

// start execution (statement, tuple)
BOOST_MYSQL_NETWORK_TEST(start_execution_statement_tuple_success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);

    // Prepare
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)").get();

    // Execute
    execution_state st;
    conn->start_execution(stmt.bind(field_view(42), field_view(40)), st).validate_no_error();
    validate_2fields_meta(st.meta(), "empty_table");
    BOOST_TEST(st.should_read_rows());
}

BOOST_MYSQL_NETWORK_TEST(start_execution_statement_tuple_error, network_fixture, err_net_samples)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare
    auto stmt = conn->prepare_statement("INSERT INTO inserts_table (field_varchar, field_date) VALUES (?, ?)")
                    .get();

    // Execute
    execution_state st;
    conn->start_execution(stmt.bind(field_view("abc"), field_view("bad_date")), st)
        .validate_error(
            common_server_errc::er_truncated_wrong_value,
            {"field_date", "bad_date", "incorrect date value"}
        );
}

// Execute (statement, iterator)
BOOST_MYSQL_NETWORK_TEST(execute_statement_iterator_success, network_fixture, err_net_samples)
{
    setup_and_connect(sample.net);

    // Prepare
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)").get();

    // Execute
    results result;
    std::forward_list<field_view> stmt_params{field_view("item"), field_view(42)};
    conn->execute(stmt.bind(stmt_params.cbegin(), stmt_params.cend()), result).validate_no_error();
    BOOST_TEST(result.rows().size() == 0u);
}

BOOST_MYSQL_NETWORK_TEST(execute_statement_error, network_fixture, err_net_samples)
{
    setup_and_connect(sample.net);
    start_transaction();

    // Prepare
    auto stmt = conn->prepare_statement("INSERT INTO inserts_table (field_varchar, field_date) VALUES (?, ?)")
                    .get();

    // Execute
    results result;
    conn->execute(stmt.bind(field_view("f0"), field_view("bad_date")), result)
        .validate_error(
            common_server_errc::er_truncated_wrong_value,
            {"field_date", "bad_date", "incorrect date value"}
        );
}

// Execute (statement, tuple). No error spotcheck since it's the same underlying fn
BOOST_MYSQL_NETWORK_TEST(execute_statement_tuple_success, network_fixture, err_net_samples)
{
    setup_and_connect(sample.net);

    // Prepare
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)").get();

    // Execute
    results result;
    conn->execute(stmt.bind(field_view("item"), field_view(42)), result).validate_no_error();
    BOOST_TEST(result.rows().size() == 0u);
}

// Close statement: no server error spotcheck
BOOST_MYSQL_NETWORK_TEST(close_statement_success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);

    // Prepare a statement
    auto stmt = conn->prepare_statement("SELECT * FROM empty_table WHERE id IN (?, ?)").get();

    // Close the statement
    conn->close_statement(stmt).validate_no_error();

    // The statement is no longer valid
    results result;
    conn->execute(stmt.bind(field_view("a"), field_view("b")), result).validate_any_error();
}

// Read some rows: no server error spotcheck
BOOST_MYSQL_NETWORK_TEST(read_some_rows_success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);

    // Generate an execution state
    execution_state st;
    conn->start_execution("SELECT * FROM one_row_table", st);
    BOOST_TEST_REQUIRE(st.should_read_rows());

    // Read once. st may or may not be complete, depending
    // on how the buffer reallocated memory
    auto rows = conn->read_some_rows(st).get();
    BOOST_TEST((rows == makerows(2, 1, "f0")));

    // Reading again should complete st
    rows = conn->read_some_rows(st).get();
    BOOST_TEST(rows.empty());
    validate_eof(st);

    // Reading again does nothing
    rows = conn->read_some_rows(st).get();
    BOOST_TEST(rows.empty());
    validate_eof(st);
}

// Read resultset head
BOOST_MYSQL_NETWORK_TEST(read_resultset_head_success, network_fixture, all_network_samples())
{
    params.set_multi_queries(true);
    setup_and_connect(sample.net);

    // Generate an execution state
    execution_state st;
    conn->start_execution("SELECT * FROM empty_table; SELECT * FROM one_row_table", st);
    BOOST_TEST_REQUIRE(st.should_read_rows());

    // Read the OK packet to finish 1st resultset
    conn->read_some_rows(st).validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_head());

    // Read head
    conn->read_resultset_head(st).validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_rows());

    // Reading head again does nothing
    conn->read_resultset_head(st).validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_rows());

    // We can read rows now
    auto rows = conn->read_some_rows(st).get();
    BOOST_TEST((rows == makerows(2, 1, "f0")));
}

BOOST_MYSQL_NETWORK_TEST(read_resultset_head_error, network_fixture, all_network_samples())
{
    params.set_multi_queries(true);
    setup_and_connect(sample.net);

    // Generate an execution state
    execution_state st;
    conn->start_execution("SELECT * FROM empty_table; SELECT bad_field FROM one_row_table", st);
    BOOST_TEST_REQUIRE(st.should_read_rows());

    // Read the OK packet to finish 1st resultset
    conn->read_some_rows(st).validate_no_error();
    BOOST_TEST_REQUIRE(st.should_read_head());

    // Read head for the 2nd resultset. This one contains an error, which is detected when reading head.
    conn->read_resultset_head(st).validate_error_exact(
        common_server_errc::er_bad_field_error,
        "Unknown column 'bad_field' in 'field list'"
    );
}

// Ping
BOOST_MYSQL_NETWORK_TEST(ping_success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);
    conn->ping().validate_no_error();
}

// TODO
BOOST_MYSQL_NETWORK_TEST(ping_error, network_fixture, all_network_samples_with_handshake())
{
    setup(sample.net);

    // Ping should return an error for an unconnected connection
    conn->ping().validate_any_error();
}

// Reset connection: no server error spotcheck.
BOOST_MYSQL_NETWORK_TEST(reset_connection_success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);

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
BOOST_MYSQL_NETWORK_TEST(quit_success, network_fixture, all_network_samples_with_handshake())
{
    setup_and_connect(sample.net);

    // Quit
    conn->quit().validate_no_error();

    // TODO: we can't just query() and expect an error, because that's not reliable as a test.
    // We should have a flag to check whether the connection is connected or not
}

// Close connection: no server error spotcheck
// TODO: all_network_samples_with_handshake
BOOST_MYSQL_NETWORK_TEST(close_connection_success, network_fixture, all_network_samples_with_handshake())
{
    setup_and_connect(sample.net);

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
BOOST_MYSQL_NETWORK_TEST(not_open_connection, network_fixture, err_net_samples)
{
    setup(sample.net);
    conn->close().validate_no_error();
    BOOST_TEST(!conn->is_open());
}

#ifdef BOOST_MYSQL_CXX14
// Execute (static) - errors are already covered by the other tests
BOOST_MYSQL_NETWORK_TEST(execute_static_success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);

    er_connection::static_results_t result;
    conn->execute("CALL sp_spotchecks()", result).get();
    BOOST_TEST(result.rows<0>().size() == 1u);
    BOOST_TEST((result.rows<0>()[0] == row_multifield{1.1f, 11, "aaa"}));
}

// start_execution, read_resultset_head, read_some_rows success
BOOST_MYSQL_NETWORK_TEST(start_execution_and_followups_static_success, network_fixture, all_network_samples())
{
    setup_and_connect(sample.net);

    er_connection::static_state_t st;

    // Start
    conn->start_execution("CALL sp_spotchecks()", st).get();
    BOOST_TEST(st.should_read_rows());

    // Read r1 rows
    std::array<row_multifield, 2> storage;
    std::size_t num_rows = conn->read_some_rows(st, storage).get();
    BOOST_TEST(num_rows == 1u);
    BOOST_TEST((storage[0] == row_multifield{1.1f, 11, "aaa"}));

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
    BOOST_TEST((storage2[0] == row_2fields{1, std::string("f0")}));

    // Ensure we're in the next resultset
    num_rows = conn->read_some_rows(st, storage2).get();
    BOOST_TEST(num_rows == 0u);
    BOOST_TEST(st.should_read_head());

    // Read r3 head (empty)
    conn->read_resultset_head(st).get();
    BOOST_TEST(st.complete());
}

// read_some_rows failure (the other error cases are already widely tested)
BOOST_MYSQL_NETWORK_TEST(read_some_rows_error, network_fixture, err_net_samples)
{
    setup_and_connect(sample.net);

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
    {"sync_errc", set_charset_netmaker::sync_errc(&any_connection::set_character_set)},
    {"sync_exc", set_charset_netmaker::sync_exc(&any_connection::set_character_set)},
    {"async_errinfo", set_charset_netmaker::async_errinfo(&any_connection::async_set_character_set, false)},
    {"async_noerrinfo", set_charset_netmaker::async_noerrinfo(&any_connection::async_set_character_set, false)
    },
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
    {"sync_errc", run_pipeline_netmaker::sync_errc(&any_connection::run_pipeline)},
    {"sync_exc", run_pipeline_netmaker::sync_exc(&any_connection::run_pipeline)},
    {"async_errinfo", run_pipeline_netmaker::async_errinfo(&any_connection::async_run_pipeline, false)},
    {"async_noerrinfo", run_pipeline_netmaker::async_noerrinfo(&any_connection::async_run_pipeline, false)},
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
