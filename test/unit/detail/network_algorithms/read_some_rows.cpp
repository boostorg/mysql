//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>

#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "creation/create_execution_state.hpp"
#include "creation/create_message.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_row_message.hpp"
#include "test_common.hpp"
#include "test_connection.hpp"
#include "unit_netfun_maker.hpp"

using boost::mysql::detail::protocol_field_type;
using namespace boost::mysql::test;
using namespace boost::mysql;

namespace {

using netfun_maker = netfun_maker_mem<rows_view, test_connection, execution_state&>;

struct
{
    typename netfun_maker::signature read_some_rows;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::read_some_rows),           "sync" },
    {netfun_maker::async_errinfo(&test_connection::async_read_some_rows), "async"},
};

execution_state create_initial_state(test_connection& conn)
{
    return exec_builder(false)
        .reset(detail::resultset_encoding::text, &get_channel(conn).shared_fields())
        .meta({protocol_field_type::var_string})
        .rows(makerows(1, 42, 50))
        .seqnum(42)
        .build_state();
}

BOOST_AUTO_TEST_SUITE(test_read_some_rows)

BOOST_AUTO_TEST_CASE(empty_resultset)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            test_connection conn;
            auto st = create_initial_state(conn);
            get_channel(conn).lowest_layer().add_message(
                ok_msg_builder().affected_rows(1).info("1st").seqnum(42).build_eof()
            );

            rows_view rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST(rv == makerows(1));
            BOOST_TEST_REQUIRE(st.complete());
            BOOST_TEST(st.affected_rows() == 1u);
            BOOST_TEST(st.info() == "1st");
            BOOST_TEST(get_channel(conn).shared_sequence_number() == 0u);  // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(batch_with_rows)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            test_connection conn;
            auto st = create_initial_state(conn);
            get_channel(conn)
                .lowest_layer()
                .add_message(
                    concat_copy(create_text_row_message(42, "abc"), create_text_row_message(43, "von"))
                )
                .add_message(create_text_row_message(44, "other"));  // only a single read should be issued

            rows_view rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST(rv == makerows(1, "abc", "von"));
            BOOST_TEST(st.should_read_rows());
            BOOST_TEST(get_channel(conn).shared_sequence_number() == 0u);  // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(batch_with_rows_eof)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            test_connection conn;
            auto st = create_initial_state(conn);
            get_channel(conn).lowest_layer().add_message(concat_copy(
                create_text_row_message(42, "abc"),
                create_text_row_message(43, "von"),
                ok_msg_builder().seqnum(44).affected_rows(1).info("1st").build_eof()
            ));

            rows_view rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST(rv == makerows(1, "abc", "von"));
            BOOST_TEST_REQUIRE(st.complete());
            BOOST_TEST(st.affected_rows() == 1u);
            BOOST_TEST(st.info() == "1st");
            BOOST_TEST(get_channel(conn).shared_sequence_number() == 0u);  // not used
        }
    }
}

// Regression check: don't attempt to continue reading after the 1st EOF for multi-result
BOOST_AUTO_TEST_CASE(batch_with_rows_eof_multiresult)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            test_connection conn;
            auto st = create_initial_state(conn);
            get_channel(conn).lowest_layer().add_message(concat_copy(
                create_text_row_message(42, "abc"),
                ok_msg_builder().seqnum(43).affected_rows(1).info("1st").more_results(true).build_eof(),
                ok_msg_builder().seqnum(44).info("2nd").build_ok()
            ));

            rows_view rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST(rv == makerows(1, "abc"));
            BOOST_TEST_REQUIRE(st.should_read_head());
            BOOST_TEST(st.affected_rows() == 1u);
            BOOST_TEST(st.info() == "1st");
        }
    }
}

// read_some_rows is a no-op if !st.should_read_rows()
BOOST_AUTO_TEST_CASE(state_complete)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            test_connection conn;
            auto st = exec_builder(false)
                          .reset(&get_channel(conn).shared_fields())
                          .meta({protocol_field_type::var_string})
                          .rows(makerows(1, 60, 70))
                          .ok(ok_builder().affected_rows(90).info("1st").build())
                          .seqnum(42)
                          .build_state();

            rows_view rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST(rv == rows());
            BOOST_TEST_REQUIRE(st.complete());
            BOOST_TEST(st.affected_rows() == 90u);
            BOOST_TEST(st.info() == "1st");
        }
    }
}

BOOST_AUTO_TEST_CASE(state_reading_head)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            test_connection conn;
            auto st = exec_builder(false)
                          .reset(&get_channel(conn).shared_fields())
                          .meta({protocol_field_type::var_string})
                          .rows(makerows(1, 60, 70))
                          .ok(ok_builder().affected_rows(90).info("1st").more_results(true).build())
                          .seqnum(42)
                          .build_state();

            rows_view rv = fns.read_some_rows(conn, st).get();
            BOOST_TEST(rv == rows());
            BOOST_TEST_REQUIRE(st.should_read_head());
            BOOST_TEST(st.affected_rows() == 90u);
            BOOST_TEST(st.info() == "1st");
        }
    }
}

BOOST_AUTO_TEST_CASE(error_network_error)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            for (std::size_t i = 0; i <= 1; ++i)
            {
                BOOST_TEST_CONTEXT("i=" << i)
                {
                    test_connection conn;
                    auto st = create_initial_state(conn);
                    get_channel(conn)
                        .lowest_layer()
                        .add_message(create_text_row_message(42, "abc"))
                        .add_message(ok_msg_builder().seqnum(43).affected_rows(1).info("1st").build_eof())
                        .set_fail_count(fail_count(i, client_errc::wrong_num_params));

                    fns.read_some_rows(conn, st).validate_error_exact(client_errc::wrong_num_params);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(error_seqnum_mismatch)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            test_connection conn;
            auto st = create_initial_state(conn);
            get_channel(conn).lowest_layer().add_message(create_text_row_message(0, "abc"));

            fns.read_some_rows(conn, st).validate_error_exact(client_errc::sequence_number_mismatch);
        }
    }
}

BOOST_AUTO_TEST_CASE(error_deserialize_row)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            test_connection conn;
            auto st = create_initial_state(conn);
            get_channel(conn).lowest_layer().add_message(create_message(42, {0x02, 0xff}));

            fns.read_some_rows(conn, st).validate_error_exact(client_errc::incomplete_message);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace