//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include <boost/mysql/mysql_collations.hpp>
#include <boost/mysql/results.hpp>

#include <boost/mysql/detail/protocol/constants.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdint>

#include "assert_buffer_equals.hpp"
#include "buffer_concat.hpp"
#include "creation/create_message.hpp"
#include "creation/create_meta.hpp"
#include "creation/create_row_message.hpp"
#include "creation/create_statement.hpp"
#include "run_coroutine.hpp"
#include "test_connection.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using boost::asio::buffer;
using boost::mysql::detail::protocol_field_type;

namespace {

BOOST_AUTO_TEST_SUITE(test_misc)

// Make sure async_query() and friends don't cause side
// effects in the initiation
#ifdef BOOST_ASIO_HAS_CO_AWAIT
BOOST_AUTO_TEST_CASE(side_effects_in_initiation)
{
    boost::asio::io_context ctx;
    test_connection conn;
    results result1, result2;

    // Resultsets will be complete as soon as a message is read
    auto ok_packet_1 = ok_msg_builder().seqnum(1).affected_rows(1).build_ok();
    auto ok_packet_2 = ok_msg_builder().seqnum(1).affected_rows(2).build_ok();
    conn.stream().add_message(ok_packet_2);
    conn.stream().add_message(ok_packet_1);

    // Launch coroutine and wait for completion
    run_coroutine([&]() -> boost::asio::awaitable<void> {
        // Call both queries but don't wait on them yet, so they don't initiate
        auto aw1 = conn.async_query("Q1", result1, boost::asio::use_awaitable);
        auto aw2 = conn.async_query("Q2", result2, boost::asio::use_awaitable);

        // Run them in reverse order
        co_await std::move(aw2);
        co_await std::move(aw1);
    });

    // Check that we wrote Q2's message first, then Q1's
    auto expected = concat_copy(
        create_message(0, {0x03, 0x51, 0x32}),  // query request Q2
        create_message(0, {0x03, 0x51, 0x31})   // query request Q1
    );
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(buffer(conn.stream().bytes_written()), buffer(expected));

    // Check that the results got the right ok_packets
    BOOST_TEST(result1.affected_rows() == 1u);
    BOOST_TEST(result2.affected_rows() == 2u);
}
#endif  // BOOST_ASIO_HAS_CO_AWAIT

// Verify that execute (dynamic interface) works when rows come in separate batches
BOOST_AUTO_TEST_CASE(execute_multiple_batches)
{
    // Setup
    results result;
    test_connection conn;

    // Message sequence (each on its own read)
    get_channel(conn)
        .lowest_layer()
        .add_message(create_message(1, {0x02}))                                  // OK, 2 columns
        .add_message(create_coldef_message(2, protocol_field_type::var_string))  // meta
        .add_message(create_coldef_message(
            3,
            coldef_builder().type(protocol_field_type::blob).collation(mysql_collations::binary).build()
        ))                                                                      // meta
        .add_message(create_text_row_message(4, "abcd", makebv("\0\1\0")))      // row 1
        .add_message(create_text_row_message(5, "defghi", makebv("\3\4\3\0")))  // row 2
        .add_message(ok_msg_builder().seqnum(6).affected_rows(10u).info("1st").more_results(true).build_eof())
        .add_message(ok_msg_builder().seqnum(7).affected_rows(20u).info("2nd").more_results(true).build_ok())
        .add_message(create_message(8, {0x01}))                                  // OK, 1 metadata
        .add_message(create_coldef_message(9, protocol_field_type::var_string))  // meta
        .add_message(create_text_row_message(10, "ab"))                          // row 1
        .add_message(ok_msg_builder().seqnum(11).affected_rows(30u).info("3rd").build_eof());

    // Call the function
    conn.execute("abc", result);

    // We've written the query request
    auto expected_msg = create_message(0, {0x03, 0x61, 0x62, 0x63});  // ASCII "abc" (plus length)
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(get_channel(conn).lowest_layer().bytes_written(), expected_msg);

    // We've populated the results
    BOOST_TEST_REQUIRE(result.size() == 3u);
    BOOST_TEST(result[0].affected_rows() == 10u);
    BOOST_TEST(result[0].info() == "1st");
    BOOST_TEST(result[0].rows() == makerows(2, "abcd", makebv("\0\1\0"), "defghi", makebv("\3\4\3\0")));
    BOOST_TEST(result[1].affected_rows() == 20u);
    BOOST_TEST(result[1].info() == "2nd");
    BOOST_TEST(result[1].rows() == rows());
    BOOST_TEST(result[2].affected_rows() == 30u);
    BOOST_TEST(result[2].info() == "3rd");
    BOOST_TEST(result[2].rows() == makerows(1, "ab"));
}

// Regression check: execute statement with iterator range with a reference type that is convertible to
// field_view, but not equal to field_view
BOOST_AUTO_TEST_CASE(execute_stmt_iterator_reference_not_field_view)
{
    results result;
    auto stmt = statement_builder().id(1).num_params(2).build();
    test_connection conn;
    conn.stream().add_message(ok_msg_builder().seqnum(1).affected_rows(50).info("1st").build_ok());

    // Call the function
    std::vector<field> fields{field_view("test"), field_view()};
    conn.execute(stmt.bind(fields.begin(), fields.end()), result);

    // Verify the message we sent
    constexpr std::uint8_t expected_msg[] = {
        0x15, 0x00, 0x00, 0x00, 0x17, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x02, 0x01, 0xfe, 0x00, 0x06, 0x00, 0x04, 0x74, 0x65, 0x73, 0x74,
    };
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(conn.stream().bytes_written(), expected_msg);

    // Verify the results
    BOOST_TEST_REQUIRE(result.size() == 1u);
    BOOST_TEST(result.meta().size() == 0u);
    BOOST_TEST(result.affected_rows() == 50u);
    BOOST_TEST(result.info() == "1st");
}

// Verify that we don't require the passed-in statement to be alive. Only
// relevant for deferred tokens.
#ifdef BOOST_ASIO_HAS_CO_AWAIT
BOOST_AUTO_TEST_CASE(close_statement_handle_deferred_tokens)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        fixture fix;

        // Deferred op
        auto aw = fix.conn.async_close_statement(statement(fix.stmt), boost::asio::use_awaitable);
        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BUFFER_EQUALS(fix.conn.stream().bytes_written(), expected_message);
    });
}
#endif

// Verify that we correctly perform a decay-copy of the execution request,
// relevant for deferred tokens
#ifdef BOOST_ASIO_HAS_CO_AWAIT
BOOST_AUTO_TEST_CASE(deferred_lifetimes_rvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        results result;
        auto chan = create_channel();
        chan.lowest_layer().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());
        diagnostics diag;

        // Deferred op. Execution request is a temporary
        auto aw = detail::async_execute(
            chan,
            create_the_statement().bind(std::string("test"), nullptr),
            get_iface(result),
            diag,
            boost::asio::use_awaitable
        );
        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);
        BOOST_TEST(result.info() == "1st");
    });
}

BOOST_AUTO_TEST_CASE(deferred_lifetimes_lvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        results result;
        auto chan = create_channel();
        diagnostics diag;
        chan.lowest_layer().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());
        boost::asio::awaitable<void> aw;

        // Deferred op
        {
            auto stmt = create_the_statement();
            auto req = stmt.bind(std::string("test"), nullptr);
            aw = detail::async_execute(chan, req, get_iface(result), diag, boost::asio::use_awaitable);
        }

        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);
        BOOST_TEST(result.info() == "1st");
    });
}
#endif
BOOST_AUTO_TEST_SUITE_END()

// Verify that we correctly perform a decay-copy of the parameters and the
// statement handle, relevant for deferred tokens
#ifdef BOOST_ASIO_HAS_CO_AWAIT
BOOST_AUTO_TEST_CASE(deferred_lifetimes_rvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        execution_state st;
        auto chan = create_channel();
        chan.lowest_layer().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());
        diagnostics diag;

        // Deferred op. Execution request is a temporary
        auto aw = detail::async_start_execution(
            chan,
            create_the_statement().bind(std::string("test"), nullptr),
            get_iface(st),
            diag,
            boost::asio::use_awaitable
        );
        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);
        BOOST_TEST(st.info() == "1st");
    });
}

BOOST_AUTO_TEST_CASE(deferred_lifetimes_lvalues)
{
    run_coroutine([]() -> boost::asio::awaitable<void> {
        execution_state st;
        auto chan = create_channel();
        chan.lowest_layer().add_message(ok_msg_builder().seqnum(1).info("1st").build_ok());
        boost::asio::awaitable<void> aw;
        diagnostics diag;

        // Deferred op
        {
            const auto stmt = create_the_statement();
            const auto req = stmt.bind(std::string("test"), nullptr);
            aw = detail::async_start_execution(chan, req, get_iface(st), diag, boost::asio::use_awaitable);
        }

        co_await std::move(aw);

        // verify that the op had the intended effects
        BOOST_MYSQL_ASSERT_BUFFER_EQUALS(chan.lowest_layer().bytes_written(), execute_stmt_msg);
        BOOST_TEST(st.info() == "1st");
    });
}
#endif
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace
