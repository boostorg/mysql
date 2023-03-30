//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/test/unit_test.hpp>

#include <tuple>

#include "assert_buffer_equals.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_statement.hpp"
#include "printing.hpp"
#include "run_coroutine.hpp"
#include "test_common.hpp"
#include "test_connection.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using detail::protocol_field_type;
using detail::resultset_encoding;

namespace {

BOOST_AUTO_TEST_SUITE(test_high_level_execution)

// The serialized form of the "SELECT 1" query
constexpr std::uint8_t select_1_msg[] =
    {0x09, 0x00, 0x00, 0x00, 0x03, 0x53, 0x45, 0x4c, 0x45, 0x43, 0x54, 0x20, 0x31};

// The serialized form of executing a statement with ID=1, params=("test", nullptr)
constexpr std::uint8_t execute_stmt_msg[] = {
    0x15, 0x00, 0x00, 0x00, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x02, 0x01, 0xfe, 0x00, 0x06, 0x00, 0x04, 0x74, 0x65, 0x73, 0x74,
};

// The statement to be executed
statement create_the_statement() { return statement_builder().id(1).num_params(2).build(); }

// Verify that we clear any previous result
results create_initial_results()
{
    return create_results({
        {
         {protocol_field_type::var_string},
         makerows(1, "abc", "def"),
         ok_builder().affected_rows(42).info("prev").build(),
         }
    });
}

execution_state create_initial_state()
{
    std::vector<field_view> fields;  // won't be further used - can be left dangling
    return exec_builder(false)
        .reset(resultset_encoding::binary, &fields)
        .meta({protocol_field_type::time})
        .seqnum(42)
        .build_state();
}

//
// ------- query ---------
//
BOOST_AUTO_TEST_SUITE(query_)

using netfun_maker = netfun_maker_mem<void, test_connection, string_view, results&>;

struct
{
    netfun_maker::signature query;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::query),             "sync_errc"      },
    {netfun_maker::sync_exc(&test_connection::query),              "sync_exc"       },
    {netfun_maker::async_errinfo(&test_connection::async_query),   "async_errinfo"  },
    {netfun_maker::async_noerrinfo(&test_connection::async_query), "async_noerrinfo"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto result = create_initial_results();
            test_connection conn;
            conn.stream().add_message(ok_msg_builder().seqnum(1).affected_rows(10).info("1st").build_ok());

            // Call the function
            fns.query(conn, "SELECT 1", result).validate_no_error();

            // Verify the message we sent
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(conn.stream().bytes_written(), select_1_msg);

            // Verify the results
            BOOST_TEST_REQUIRE(result.size() == 1u);
            BOOST_TEST(result.meta().size() == 0u);
            BOOST_TEST(result.affected_rows() == 10u);
            BOOST_TEST(result.info() == "1st");
        }
    }
}

BOOST_AUTO_TEST_CASE(error)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            results result;
            test_connection conn;
            conn.stream().set_fail_count(fail_count(0, common_server_errc::er_aborting_connection));

            // Call the function
            fns.query(conn, "SELECT 1", result)
                .validate_error_exact(common_server_errc::er_aborting_connection);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

//
// ------- start_query ---------
//
BOOST_AUTO_TEST_SUITE(start_query_)

using netfun_maker = netfun_maker_mem<void, test_connection, string_view, execution_state&>;

struct
{
    netfun_maker::signature start_query;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::start_query),             "sync_errc"      },
    {netfun_maker::sync_exc(&test_connection::start_query),              "sync_exc"       },
    {netfun_maker::async_errinfo(&test_connection::async_start_query),   "async_errinfo"  },
    {netfun_maker::async_noerrinfo(&test_connection::async_start_query), "async_noerrinfo"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            std::vector<field_view> fields;
            auto st = create_initial_state();
            test_connection conn;
            conn.stream().add_message(ok_msg_builder().seqnum(1).affected_rows(50).info("1st").build_ok());

            // Call the function
            fns.start_query(conn, "SELECT 1", st).validate_no_error();

            // Verify the message we sent
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(conn.stream().bytes_written(), select_1_msg);

            // Verify the results
            BOOST_TEST(get_impl(st).encoding() == resultset_encoding::text);
            BOOST_TEST(st.complete());
            BOOST_TEST(get_impl(st).sequence_number() == 2u);
            BOOST_TEST(st.meta().size() == 0u);
            BOOST_TEST(st.affected_rows() == 50u);
            BOOST_TEST(st.info() == "1st");
        }
    }
}

BOOST_AUTO_TEST_CASE(error)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            execution_state st;
            test_connection conn;
            conn.stream().set_fail_count(fail_count(0, common_server_errc::er_aborting_connection));

            // Call the function
            fns.start_query(conn, "SELECT 1", st)
                .validate_error_exact(common_server_errc::er_aborting_connection);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

//
// ------- execute_statement ---------
//
BOOST_AUTO_TEST_SUITE(execute_statement_)

using netfun_maker = netfun_maker_mem<
    void,
    test_connection,
    const statement&,
    const std::tuple<const char*, std::nullptr_t>&,
    results&>;

struct
{
    netfun_maker::signature execute_statement;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::execute_statement),             "sync_errc"      },
    {netfun_maker::sync_exc(&test_connection::execute_statement),              "sync_exc"       },
    {netfun_maker::async_errinfo(&test_connection::async_execute_statement),   "async_errinfo"  },
    {netfun_maker::async_noerrinfo(&test_connection::async_execute_statement), "async_noerrinfo"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto result = create_initial_results();
            auto stmt = create_the_statement();
            test_connection conn;
            conn.stream().add_message(ok_msg_builder().seqnum(1).affected_rows(50).info("1st").build_ok());

            // Call the function
            fns.execute_statement(conn, stmt, std::make_tuple("test", nullptr), result).validate_no_error();

            // Verify the message we sent
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(conn.stream().bytes_written(), execute_stmt_msg);

            // Verify the results
            BOOST_TEST_REQUIRE(result.size() == 1u);
            BOOST_TEST(result.meta().size() == 0u);
            BOOST_TEST(result.affected_rows() == 50u);
            BOOST_TEST(result.info() == "1st");
        }
    }
}

BOOST_AUTO_TEST_CASE(error_wrong_num_params)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto result = create_initial_results();
            auto stmt = statement_builder().id(1).num_params(3).build();
            test_connection conn;

            // Call the function
            fns.execute_statement(conn, stmt, std::make_tuple("test", nullptr), result)
                .validate_error_exact(client_errc::wrong_num_params);
        }
    }
}

// Verify that we correctly perform a decay-copy of the parameters and the
// statement handle, relevant for deferred tokens
#ifdef BOOST_ASIO_HAS_CO_AWAIT
BOOST_AUTO_TEST_CASE(deferred_lifetimes_rvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        results result;
        test_connection conn;
        conn.stream().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());

        // Deferred op
        auto aw = conn.async_execute_statement(
            create_the_statement(),                         // statement is a temporary
            std::make_tuple(std::string("test"), nullptr),  // tuple is a temporary
            result,
            boost::asio::use_awaitable
        );
        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BLOB_EQUALS(conn.stream().bytes_written(), execute_stmt_msg);
        BOOST_TEST(result.info() == "1st");
    });
}

BOOST_AUTO_TEST_CASE(deferred_lifetimes_lvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        results result;
        test_connection conn;
        conn.stream().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());
        boost::asio::awaitable<void> aw;

        // Deferred op
        {
            auto stmt = create_the_statement();
            auto params = std::make_tuple(std::string("test"), nullptr);
            aw = conn.async_execute_statement(stmt, params, result, boost::asio::use_awaitable);
        }

        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BLOB_EQUALS(conn.stream().bytes_written(), execute_stmt_msg);
        BOOST_TEST(result.info() == "1st");
    });
}
#endif

BOOST_AUTO_TEST_SUITE_END()

//
// ------- start_statement_execution (tuple) ---------
//
BOOST_AUTO_TEST_SUITE(start_statement_execution_tuple)

using netfun_maker = netfun_maker_mem<
    void,
    test_connection,
    const statement&,
    const std::tuple<const char*, std::nullptr_t>&,
    execution_state&>;

struct
{
    netfun_maker::signature start_statement_execution;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::start_statement_execution),             "sync_errc"      },
    {netfun_maker::sync_exc(&test_connection::start_statement_execution),              "sync_exc"       },
    {netfun_maker::async_errinfo(&test_connection::async_start_statement_execution),   "async_errinfo"  },
    {netfun_maker::async_noerrinfo(&test_connection::async_start_statement_execution), "async_noerrinfo"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto st = create_initial_state();
            auto stmt = create_the_statement();
            test_connection conn;
            conn.stream().add_message(ok_msg_builder().seqnum(1).affected_rows(50).info("1st").build_ok());

            // Call the function
            fns.start_statement_execution(conn, stmt, std::make_tuple("test", nullptr), st)
                .validate_no_error();

            // Verify the message we sent
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(conn.stream().bytes_written(), execute_stmt_msg);

            // Verify the results
            BOOST_TEST(get_impl(st).encoding() == resultset_encoding::binary);
            BOOST_TEST_REQUIRE(st.complete());
            BOOST_TEST(get_impl(st).sequence_number() == 2u);
            BOOST_TEST(st.meta().size() == 0u);
            BOOST_TEST(st.affected_rows() == 50u);
            BOOST_TEST(st.info() == "1st");
        }
    }
}

BOOST_AUTO_TEST_CASE(error_wrong_num_params)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            execution_state st;
            auto stmt = statement_builder().id(1).num_params(3).build();
            test_connection conn;

            // Call the function
            fns.start_statement_execution(conn, stmt, std::make_tuple("test", nullptr), st)
                .validate_error_exact(client_errc::wrong_num_params);
        }
    }
}

// Verify that we correctly perform a decay-copy of the parameters and the
// statement handle, relevant for deferred tokens
#ifdef BOOST_ASIO_HAS_CO_AWAIT
BOOST_AUTO_TEST_CASE(deferred_lifetimes_rvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        execution_state st;
        test_connection conn;
        conn.stream().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());

        // Deferred op
        auto aw = conn.async_start_statement_execution(
            create_the_statement(),                         // statement is a temporary
            std::make_tuple(std::string("test"), nullptr),  // tuple is a temporary
            st,
            boost::asio::use_awaitable
        );
        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BLOB_EQUALS(conn.stream().bytes_written(), execute_stmt_msg);
        BOOST_TEST(st.info() == "1st");
    });
}

BOOST_AUTO_TEST_CASE(deferred_lifetimes_lvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        execution_state st;
        test_connection conn;
        conn.stream().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());
        boost::asio::awaitable<void> aw;

        // Deferred op
        {
            auto stmt = create_the_statement();
            auto params = std::make_tuple(std::string("test"), nullptr);
            aw = conn.async_start_statement_execution(stmt, params, st, boost::asio::use_awaitable);
        }

        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BLOB_EQUALS(conn.stream().bytes_written(), execute_stmt_msg);
        BOOST_TEST(st.info() == "1st");
    });
}
#endif

BOOST_AUTO_TEST_SUITE_END()

//
// ------- start_statement_execution (iterator) ---------
//
BOOST_AUTO_TEST_SUITE(start_statement_execution_it)

using netfun_maker = netfun_maker_mem<
    void,
    test_connection,
    const statement&,
    const field_view*,
    const field_view*,
    execution_state&>;

struct
{
    netfun_maker::signature start_statement_execution;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::start_statement_execution),             "sync_errc"      },
    {netfun_maker::sync_exc(&test_connection::start_statement_execution),              "sync_exc"       },
    {netfun_maker::async_errinfo(&test_connection::async_start_statement_execution),   "async_errinfo"  },
    {netfun_maker::async_noerrinfo(&test_connection::async_start_statement_execution), "async_noerrinfo"},
};

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto st = create_initial_state();
            auto stmt = create_the_statement();
            test_connection conn;
            conn.stream().add_message(ok_msg_builder().seqnum(1).affected_rows(50).info("1st").build_ok());

            // Call the function
            auto fields = make_fv_arr("test", nullptr);
            fns.start_statement_execution(conn, stmt, fields.data(), fields.data() + fields.size(), st)
                .validate_no_error();

            // Verify the message we sent
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(conn.stream().bytes_written(), execute_stmt_msg);

            // Verify the results
            BOOST_TEST(get_impl(st).encoding() == resultset_encoding::binary);
            BOOST_TEST(st.complete());
            BOOST_TEST(get_impl(st).sequence_number() == 2u);
            BOOST_TEST(st.meta().size() == 0u);
            BOOST_TEST(st.affected_rows() == 50u);
            BOOST_TEST(st.info() == "1st");
        }
    }
}

BOOST_AUTO_TEST_CASE(error_wrong_num_params)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            execution_state st;
            auto stmt = statement_builder().id(1).num_params(3).build();
            test_connection conn;

            // Call the function
            auto fields = make_fv_arr("test", nullptr);
            fns.start_statement_execution(conn, stmt, fields.data(), fields.data() + fields.size(), st)
                .validate_error_exact(client_errc::wrong_num_params);
        }
    }
}

// Verify that we correctly perform a decay-copy of the stmt handle
#ifdef BOOST_ASIO_HAS_CO_AWAIT
BOOST_AUTO_TEST_CASE(deferred_lifetimes)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        execution_state st;
        test_connection conn;
        conn.stream().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());
        auto fields = make_fv_arr("test", nullptr);

        // Deferred op
        auto aw = conn.async_start_statement_execution(
            create_the_statement(),
            fields.data(),
            fields.data() + fields.size(),
            st,
            boost::asio::use_awaitable
        );
        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BLOB_EQUALS(conn.stream().bytes_written(), execute_stmt_msg);
        BOOST_TEST(st.info() == "1st");
    });
}
#endif

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace