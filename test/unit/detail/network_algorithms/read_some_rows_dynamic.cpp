//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows_dynamic.hpp>

#include <boost/test/unit_test.hpp>

#include "creation/create_execution_processor.hpp"
#include "creation/create_message.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_meta.hpp"
#include "creation/create_row_message.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using boost::mysql::detail::execution_state_impl;

namespace {

BOOST_AUTO_TEST_SUITE(test_read_some_rows_dynamic)

using netfun_maker = netfun_maker_fn<rows_view, test_channel&, execution_state_impl&>;

struct
{
    typename netfun_maker::signature read_some_rows_dynamic;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::read_some_rows_dynamic),           "sync" },
    {netfun_maker::async_errinfo(&detail::async_read_some_rows_dynamic), "async"},
};

struct fixture
{
    execution_state_impl st;
    test_channel chan;

    fixture() : chan(create_channel())
    {
        // Prepare the state, such that it's ready to read rows
        add_meta(st, {meta_builder().type(column_type::varchar).build()});
        st.sequence_number() = 42;

        // Put something in shared_fields, simulating a previous read
        chan.shared_fields().push_back(field_view("prev"));  // from previous read
    }
};

BOOST_AUTO_TEST_CASE(eof)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            fix.chan.lowest_layer().add_message(
                ok_msg_builder().affected_rows(1).info("1st").seqnum(42).build_eof()
            );

            rows_view rv = fns.read_some_rows_dynamic(fix.chan, fix.st).get();
            BOOST_TEST(rv == makerows(1));
            BOOST_TEST_REQUIRE(fix.st.is_complete());
            BOOST_TEST(fix.st.get_affected_rows() == 1u);
            BOOST_TEST(fix.st.get_info() == "1st");
            BOOST_TEST(fix.chan.shared_sequence_number() == 0u);  // not used
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
            fix.chan.lowest_layer()
                .add_message(
                    concat_copy(create_text_row_message(42, "abc"), create_text_row_message(43, "von"))
                )
                .add_message(create_text_row_message(44, "other"));  // only a single read should be issued

            rows_view rv = fns.read_some_rows_dynamic(fix.chan, fix.st).get();
            BOOST_TEST(rv == makerows(1, "abc", "von"));
            BOOST_TEST(fix.st.is_reading_rows());
            BOOST_TEST(fix.chan.shared_sequence_number() == 0u);  // not used
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
            fix.chan.lowest_layer().add_message(concat_copy(
                create_text_row_message(42, "abc"),
                create_text_row_message(43, "von"),
                ok_msg_builder().seqnum(44).affected_rows(1).info("1st").build_eof()
            ));

            rows_view rv = fns.read_some_rows_dynamic(fix.chan, fix.st).get();
            BOOST_TEST(rv == makerows(1, "abc", "von"));
            BOOST_TEST_REQUIRE(fix.st.is_complete());
            BOOST_TEST(fix.st.get_affected_rows() == 1u);
            BOOST_TEST(fix.st.get_info() == "1st");
            BOOST_TEST(fix.chan.shared_sequence_number() == 0u);  // not used
        }
    }
}

// All the other error cases are already tested in read_some_rows_impl. Spotcheck
BOOST_AUTO_TEST_CASE(error)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;

            // invalid row
            fix.chan.lowest_layer().add_message(create_message(42, {0x02, 0xff}));

            fns.read_some_rows_dynamic(fix.chan, fix.st)
                .validate_error_exact(client_errc::incomplete_message);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace