//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Since integration tests can't reliably test multifunction operations
// that span over multiple messages, we test the complete multifn fllow in this unit tests.

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/rows_view.hpp>

#include <boost/mysql/detail/protocol/constants.hpp>

#include <boost/test/unit_test.hpp>

#include "assert_buffer_equals.hpp"
#include "check_meta.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message.hpp"
#include "creation/create_row_message.hpp"
#include "printing.hpp"
#include "test_common.hpp"
#include "test_connection.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using detail::protocol_field_type;

namespace {

using start_query_netm = netfun_maker_mem<void, test_connection, string_view, execution_state&>;
using read_resultset_head_netm = netfun_maker_mem<void, test_connection, execution_state&>;
using read_some_rows_netm = netfun_maker_mem<rows_view, test_connection, execution_state&>;

struct
{
    start_query_netm::signature start_query;
    read_resultset_head_netm::signature read_resultset_head;
    read_some_rows_netm::signature read_some_rows;
    const char* name;
} all_fns[] = {
    {start_query_netm::sync_errc(&test_connection::start_query),
     read_resultset_head_netm::sync_errc(&test_connection::read_resultset_head),
     read_some_rows_netm::sync_errc(&test_connection::read_some_rows),
     "sync" },
    {start_query_netm::async_errinfo(&test_connection::async_start_query),
     read_resultset_head_netm::async_errinfo(&test_connection::async_read_resultset_head),
     read_some_rows_netm::async_errinfo(&test_connection::async_read_some_rows),
     "async"},
};

BOOST_AUTO_TEST_SUITE(test_multifn)

BOOST_AUTO_TEST_CASE(separate_batches)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            execution_state st;
            test_connection conn;
            conn.stream()
                .add_message(create_message(1, {0x01}))
                .add_message(create_coldef_message(2, protocol_field_type::var_string))
                .add_message(create_text_row_message(3, "abc"))
                .add_message(
                    ok_msg_builder().seqnum(4).affected_rows(10u).info("1st").more_results(true).build_eof()
                )
                .add_message(create_message(5, {0x01}))
                .add_message(create_coldef_message(6, protocol_field_type::newdecimal))
                .add_message(concat_copy(create_text_row_message(7, "ab"), create_text_row_message(8, "plo")))
                .add_message(concat_copy(
                    create_text_row_message(9, "hju"),
                    ok_msg_builder().seqnum(10).affected_rows(30u).info("2nd").build_eof()
                ));

            // Start
            fns.start_query(conn, "SELECT 1", st).validate_no_error();
            BOOST_TEST_REQUIRE(st.should_read_rows());
            check_meta(st.meta(), {column_type::varchar});

            // 1st resultset, row
            auto rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST_REQUIRE(st.should_read_rows());
            BOOST_TEST(rv == makerows(1, "abc"));

            // 1st resultset, eof
            rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST_REQUIRE(st.should_read_head());
            BOOST_TEST(rv == makerows(1));
            BOOST_TEST(st.affected_rows() == 10u);
            BOOST_TEST(st.info() == "1st");

            // 2nd resultset, head
            fns.read_resultset_head(conn, st).validate_no_error();
            BOOST_TEST_REQUIRE(st.should_read_rows());
            check_meta(st.meta(), {column_type::decimal});

            // 2nd resultset, row batch
            rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST_REQUIRE(st.should_read_rows());
            BOOST_TEST(rv == makerows(1, "ab", "plo"));

            // 2nd resultset, last row & eof
            rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST_REQUIRE(st.complete());
            BOOST_TEST(rv == makerows(1, "hju"));
            BOOST_TEST(st.affected_rows() == 30u);
            BOOST_TEST(st.info() == "2nd");
        }
    }
}

// The server sent us a single, big message with everything
BOOST_AUTO_TEST_CASE(single_read)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            execution_state st;
            test_connection conn(buffer_params(4096));
            std::vector<std::uint8_t> msgs;
            concat(msgs, create_message(1, {0x01}));
            concat(msgs, create_coldef_message(2, protocol_field_type::var_string));
            concat(msgs, create_text_row_message(3, "abc"));
            concat(
                msgs,
                ok_msg_builder().seqnum(4).affected_rows(10u).info("1st").more_results(true).build_eof()
            );
            concat(msgs, create_message(5, {0x01}));
            concat(msgs, create_coldef_message(6, protocol_field_type::newdecimal));
            concat(msgs, create_text_row_message(7, "ab"));
            concat(msgs, create_text_row_message(8, "plo"));
            concat(msgs, create_text_row_message(9, "hju"));
            concat(msgs, ok_msg_builder().seqnum(10).affected_rows(30u).info("2nd").build_eof());
            conn.stream().add_message(std::move(msgs));

            // Start
            fns.start_query(conn, "SELECT 1", st).validate_no_error();
            BOOST_TEST_REQUIRE(st.should_read_rows());
            check_meta(st.meta(), {column_type::varchar});

            // First resultset
            auto rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST_REQUIRE(st.should_read_head());
            BOOST_TEST(rv == makerows(1, "abc"));
            BOOST_TEST(st.affected_rows() == 10u);
            BOOST_TEST(st.info() == "1st");

            // 2nd resultset, head
            fns.read_resultset_head(conn, st).validate_no_error();
            BOOST_TEST_REQUIRE(st.should_read_rows());
            check_meta(st.meta(), {column_type::decimal});

            // 2nd resultset
            rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST_REQUIRE(st.complete());
            BOOST_TEST(rv == makerows(1, "ab", "plo", "hju"));
            BOOST_TEST(st.affected_rows() == 30u);
            BOOST_TEST(st.info() == "2nd");
        }
    }
}

BOOST_AUTO_TEST_CASE(empty_resultsets)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            execution_state st;
            test_connection conn(buffer_params(4096));
            std::vector<std::uint8_t> msgs;
            concat(
                msgs,
                ok_msg_builder().seqnum(1).affected_rows(10u).info("1st").more_results(true).build_ok()
            );
            concat(
                msgs,
                ok_msg_builder().seqnum(2).affected_rows(20u).info("2nd").more_results(true).build_ok()
            );
            concat(msgs, ok_msg_builder().seqnum(3).affected_rows(30u).info("3rd").build_ok());
            conn.stream().add_message(std::move(msgs));

            // Start
            fns.start_query(conn, "SELECT 1", st).validate_no_error();
            BOOST_TEST_REQUIRE(st.should_read_head());
            BOOST_TEST(st.meta().size() == 0u);
            BOOST_TEST(st.affected_rows() == 10u);
            BOOST_TEST(st.info() == "1st");

            // 2nd resultset
            fns.read_resultset_head(conn, st).validate_no_error();
            BOOST_TEST_REQUIRE(st.should_read_head());
            BOOST_TEST(st.meta().size() == 0u);
            BOOST_TEST(st.affected_rows() == 20u);
            BOOST_TEST(st.info() == "2nd");

            // 3rd resultset
            fns.read_resultset_head(conn, st).validate_no_error();
            BOOST_TEST_REQUIRE(st.complete());
            BOOST_TEST(st.meta().size() == 0u);
            BOOST_TEST(st.affected_rows() == 30u);
            BOOST_TEST(st.info() == "3rd");
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace