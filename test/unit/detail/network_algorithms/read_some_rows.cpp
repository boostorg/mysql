//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/static_execution_state.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/core/span.hpp>
#include <boost/describe/class.hpp>
#include <boost/describe/operators.hpp>
#include <boost/test/unit_test.hpp>

#include "creation/create_execution_state.hpp"
#include "creation/create_message.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_meta.hpp"
#include "creation/create_row_message.hpp"
#include "creation/create_static_execution_state.hpp"
#include "test_common.hpp"
#include "test_connection.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using boost::span;

namespace {

BOOST_AUTO_TEST_SUITE(test_read_some_rows)
BOOST_AUTO_TEST_SUITE(dynamic_iface)

using netfun_maker = netfun_maker_mem<rows_view, test_connection, execution_state&>;

struct
{
    typename netfun_maker::signature read_some_rows;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::read_some_rows),             "sync_errc"      },
    {netfun_maker::sync_exc(&test_connection::read_some_rows),              "sync_exc"       },
    {netfun_maker::async_errinfo(&test_connection::async_read_some_rows),   "async_errinfo"  },
    {netfun_maker::async_noerrinfo(&test_connection::async_read_some_rows), "async_noerrinfo"},
};

struct fixture
{
    execution_state st;
    test_connection conn;

    fixture()
    {
        // Prepare the state, such that it's ready to read rows
        add_meta(get_iface(st), {meta_builder().type(column_type::varchar).build()});
        get_iface(st).sequence_number() = 42;

        // Put something in shared_fields, simulating a previous read
        get_channel(conn).shared_fields().push_back(field_view("prev"));  // from previous read
    }
};

BOOST_AUTO_TEST_CASE(empty_resultset)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            get_channel(fix.conn).lowest_layer().add_message(
                ok_msg_builder().affected_rows(1).info("1st").seqnum(42).build_eof()
            );

            rows_view rv = fns.read_some_rows(fix.conn, fix.st).get();
            BOOST_TEST(rv == makerows(1));
            BOOST_TEST_REQUIRE(fix.st.complete());
            BOOST_TEST(fix.st.affected_rows() == 1u);
            BOOST_TEST(fix.st.info() == "1st");
            BOOST_TEST(get_channel(fix.conn).shared_sequence_number() == 0u);  // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(batch_with_rows)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            get_channel(fix.conn)
                .lowest_layer()
                .add_message(
                    concat_copy(create_text_row_message(42, "abc"), create_text_row_message(43, "von"))
                )
                .add_message(create_text_row_message(44, "other"));  // only a single read should be issued

            rows_view rv = fns.read_some_rows(fix.conn, fix.st).get();
            BOOST_TEST(rv == makerows(1, "abc", "von"));
            BOOST_TEST(fix.st.should_read_rows());
            BOOST_TEST(get_channel(fix.conn).shared_sequence_number() == 0u);  // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(batch_with_rows_eof)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            get_channel(fix.conn).lowest_layer().add_message(concat_copy(
                create_text_row_message(42, "abc"),
                create_text_row_message(43, "von"),
                ok_msg_builder().seqnum(44).affected_rows(1).info("1st").build_eof()
            ));

            rows_view rv = fns.read_some_rows(fix.conn, fix.st).get();
            BOOST_TEST(rv == makerows(1, "abc", "von"));
            BOOST_TEST_REQUIRE(fix.st.complete());
            BOOST_TEST(fix.st.affected_rows() == 1u);
            BOOST_TEST(fix.st.info() == "1st");
            BOOST_TEST(get_channel(fix.conn).shared_sequence_number() == 0u);  // not used
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
            fixture fix;
            get_channel(fix.conn).lowest_layer().add_message(concat_copy(
                create_text_row_message(42, "abc"),
                ok_msg_builder().seqnum(43).affected_rows(1).info("1st").more_results(true).build_eof(),
                ok_msg_builder().seqnum(44).info("2nd").build_ok()
            ));

            rows_view rv = fns.read_some_rows(fix.conn, fix.st).get();
            BOOST_TEST(rv == makerows(1, "abc"));
            BOOST_TEST_REQUIRE(fix.st.should_read_head());
            BOOST_TEST(fix.st.affected_rows() == 1u);
            BOOST_TEST(fix.st.info() == "1st");
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
            fixture fix;
            add_ok(get_iface(fix.st), ok_builder().affected_rows(42).build());

            rows_view rv = fns.read_some_rows(fix.conn, fix.st).get();
            BOOST_TEST(rv == rows());
            BOOST_TEST_REQUIRE(fix.st.complete());
            BOOST_TEST(fix.st.affected_rows() == 42u);
        }
    }
}

BOOST_AUTO_TEST_CASE(state_reading_head)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            add_ok(get_iface(fix.st), ok_builder().affected_rows(42).more_results(true).build());

            rows_view rv = fns.read_some_rows(fix.conn, fix.st).get();
            BOOST_TEST(rv == rows());
            BOOST_TEST_REQUIRE(fix.st.should_read_head());
            BOOST_TEST(fix.st.affected_rows() == 42u);
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
                    fixture fix;
                    get_channel(fix.conn)
                        .lowest_layer()
                        .add_message(create_text_row_message(42, "abc"))
                        .add_message(ok_msg_builder().seqnum(43).affected_rows(1).info("1st").build_eof())
                        .set_fail_count(fail_count(i, client_errc::wrong_num_params));

                    fns.read_some_rows(fix.conn, fix.st).validate_error_exact(client_errc::wrong_num_params);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(error_processing_row)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;

            // invalid row
            get_channel(fix.conn).lowest_layer().add_message(create_message(42, {0x02, 0xff}));

            fns.read_some_rows(fix.conn, fix.st).validate_error_exact(client_errc::incomplete_message);
        }
    }
}
BOOST_AUTO_TEST_SUITE_END()

#ifdef BOOST_MYSQL_CXX14
BOOST_AUTO_TEST_SUITE(static_iface)

struct row1
{
    std::string fvarchar;
};
BOOST_DESCRIBE_STRUCT(row1, (), (fvarchar));

using boost::describe::operators::operator==;
using boost::describe::operators::operator<<;

using state_t = static_execution_state<row1, std::tuple<int>>;
using netfun_maker = netfun_maker_mem<std::size_t, test_connection, state_t&, span<row1>>;

struct
{
    typename netfun_maker::signature read_some_rows;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&test_connection::read_some_rows),             "sync_errc"      },
    {netfun_maker::sync_exc(&test_connection::read_some_rows),              "sync_exc"       },
    {netfun_maker::async_errinfo(&test_connection::async_read_some_rows),   "async_errinfo"  },
    {netfun_maker::async_noerrinfo(&test_connection::async_read_some_rows), "async_noerrinfo"},
};

struct fixture
{
    state_t st;
    test_connection conn;
    std::array<row1, 3> storage;

    fixture()
    {
        // Prepare the state, such that it's ready to read rows
        add_meta(
            get_iface(st),
            {meta_builder().type(column_type::varchar).name("fvarchar").nullable(false).build()}
        );
        get_iface(st).sequence_number() = 42;
    }
};

BOOST_AUTO_TEST_CASE(empty_resultset)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            get_channel(fix.conn).lowest_layer().add_message(
                ok_msg_builder().affected_rows(1).info("1st").seqnum(42).more_results(true).build_eof()
            );

            std::size_t num_rows = fns.read_some_rows(fix.conn, fix.st, fix.storage).get();
            BOOST_TEST(num_rows == 0u);
            BOOST_TEST_REQUIRE(fix.st.should_read_head());
            BOOST_TEST(fix.st.affected_rows() == 1u);
            BOOST_TEST(fix.st.info() == "1st");
            BOOST_TEST(get_channel(fix.conn).shared_sequence_number() == 0u);  // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(batch_with_rows)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            get_channel(fix.conn)
                .lowest_layer()
                .add_message(
                    concat_copy(create_text_row_message(42, "abc"), create_text_row_message(43, "von"))
                )
                .add_message(create_text_row_message(44, "other"));  // only a single read should be issued

            std::size_t num_rows = fns.read_some_rows(fix.conn, fix.st, fix.storage).get();
            BOOST_TEST(num_rows == 2u);
            BOOST_TEST(fix.storage[0].fvarchar == "abc");
            BOOST_TEST(fix.storage[1].fvarchar == "von");
            BOOST_TEST(fix.st.should_read_rows());
            BOOST_TEST(get_channel(fix.conn).shared_sequence_number() == 0u);  // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(batch_with_rows_eof)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            get_channel(fix.conn).lowest_layer().add_message(concat_copy(
                create_text_row_message(42, "abc"),
                create_text_row_message(43, "von"),
                ok_msg_builder().seqnum(44).affected_rows(1).info("1st").more_results(true).build_eof()
            ));

            std::size_t num_rows = fns.read_some_rows(fix.conn, fix.st, fix.storage).get();
            BOOST_TEST(num_rows == 2u);
            BOOST_TEST(fix.storage[0].fvarchar == "abc");
            BOOST_TEST(fix.storage[1].fvarchar == "von");
            BOOST_TEST_REQUIRE(fix.st.should_read_head());
            BOOST_TEST(fix.st.affected_rows() == 1u);
            BOOST_TEST(fix.st.info() == "1st");
            BOOST_TEST(get_channel(fix.conn).shared_sequence_number() == 0u);  // not used
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
            fixture fix;
            get_channel(fix.conn).lowest_layer().add_message(concat_copy(
                create_text_row_message(42, "abc"),
                ok_msg_builder().seqnum(43).affected_rows(1).info("1st").more_results(true).build_eof(),
                ok_msg_builder().seqnum(44).info("2nd").build_ok()
            ));

            std::size_t num_rows = fns.read_some_rows(fix.conn, fix.st, fix.storage).get();
            BOOST_TEST(num_rows == 1u);
            BOOST_TEST(fix.storage[0].fvarchar == "abc");
            BOOST_TEST_REQUIRE(fix.st.should_read_head());
            BOOST_TEST(fix.st.affected_rows() == 1u);
            BOOST_TEST(fix.st.info() == "1st");
        }
    }
}

BOOST_AUTO_TEST_CASE(batch_with_rows_out_of_span_space)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            get_channel(fix.conn).lowest_layer().add_message(concat_copy(
                create_text_row_message(42, "aaa"),
                create_text_row_message(43, "bbb"),
                create_text_row_message(44, "ccc"),
                create_text_row_message(45, "ddd")
            ));

            // We only have space for 3
            std::size_t num_rows = fns.read_some_rows(fix.conn, fix.st, fix.storage).get();
            BOOST_TEST(num_rows == 3u);
            BOOST_TEST(fix.storage[0].fvarchar == "aaa");
            BOOST_TEST(fix.storage[1].fvarchar == "bbb");
            BOOST_TEST(fix.storage[2].fvarchar == "ccc");
            BOOST_TEST(fix.st.should_read_rows());

            // Reading again reads the 4th
            num_rows = fns.read_some_rows(fix.conn, fix.st, fix.storage).get();
            BOOST_TEST(num_rows == 1u);
            BOOST_TEST(fix.storage[0].fvarchar == "ddd");
        }
    }
}

// Edge case: the query contains fields but the type doesn't
BOOST_AUTO_TEST_CASE(empty_rows)
{
    static_execution_state<std::tuple<>> st;
    test_connection conn;
    std::array<std::tuple<>, 3> storage;

    add_meta(get_iface(st), {meta_builder().type(column_type::varchar).nullable(false).build()});
    get_channel(conn).lowest_layer().add_message(create_text_row_message(0, "aaa"));

    std::size_t num_rows = conn.read_some_rows(st, span<std::tuple<>>(storage));
    BOOST_TEST(num_rows == 1u);
    BOOST_TEST(st.should_read_rows());
}

// read_some_rows is a no-op if !st.should_read_rows()
BOOST_AUTO_TEST_CASE(state_complete)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            add_ok(get_iface(fix.st), ok_builder().affected_rows(20).more_results(true).build());
            add_meta(get_iface(fix.st), {meta_builder().type(column_type::int_).nullable(false).build()});
            add_ok(get_iface(fix.st), ok_builder().affected_rows(42).build());

            std::size_t num_rows = fns.read_some_rows(fix.conn, fix.st, fix.storage).get();
            BOOST_TEST(num_rows == 0u);
            BOOST_TEST_REQUIRE(fix.st.complete());
            BOOST_TEST(fix.st.affected_rows() == 42u);
        }
    }
}

BOOST_AUTO_TEST_CASE(state_reading_head)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            add_ok(get_iface(fix.st), ok_builder().affected_rows(42).more_results(true).build());

            std::size_t num_rows = fns.read_some_rows(fix.conn, fix.st, fix.storage).get();
            BOOST_TEST(num_rows == 0u);
            BOOST_TEST_REQUIRE(fix.st.should_read_head());
            BOOST_TEST(fix.st.affected_rows() == 42u);
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
                    fixture fix;
                    get_channel(fix.conn)
                        .lowest_layer()
                        .add_message(create_text_row_message(42, "abc"))
                        .add_message(ok_msg_builder().seqnum(43).affected_rows(1).info("1st").build_eof())
                        .set_fail_count(fail_count(i, client_errc::wrong_num_params));

                    fns.read_some_rows(fix.conn, fix.st, fix.storage)
                        .validate_error_exact(client_errc::wrong_num_params);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(error_processing_row)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            get_channel(fix.conn).lowest_layer().add_message(create_text_row_message(42, 10));

            // Advance st to the next resultset
            add_ok(get_iface(fix.st), ok_builder().more_results(true).build());
            add_meta(get_iface(fix.st), {meta_builder().type(column_type::int_).nullable(false).build()});

            // The provided reference has the wrong type (we're in the 2nd resultset)
            fns.read_some_rows(fix.conn, fix.st, fix.storage)
                .validate_error_exact(client_errc::row_type_mismatch);
        }
    }
}
BOOST_AUTO_TEST_SUITE_END()
#endif

BOOST_AUTO_TEST_SUITE_END()

}  // namespace