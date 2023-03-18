//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/blob.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>

#include <boost/mysql/detail/network_algorithms/start_execution.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "assert_buffer_equals.hpp"
#include "check_meta.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message.hpp"
#include "printing.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"
#include "unit_netfun_maker.hpp"

using boost::mysql::blob;
using boost::mysql::client_errc;
using boost::mysql::column_type;
using boost::mysql::error_code;
using boost::mysql::execution_state;
using boost::mysql::field_view;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::resultset_encoding;
using namespace boost::mysql::test;

namespace {

using netfun_maker = netfun_maker_fn<void, test_channel&, resultset_encoding, execution_state&>;

struct
{
    typename netfun_maker::signature start_execution;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&boost::mysql::detail::start_execution),           "sync" },
    {netfun_maker::async_errinfo(&boost::mysql::detail::async_start_execution), "async"}
};

BOOST_AUTO_TEST_SUITE(test_start_execution)

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // Initial state, to verify that we reset it
            std::vector<field_view> fields;
            auto st = exec_builder(false)
                          .reset(resultset_encoding::text, &fields)
                          .meta({protocol_field_type::geometry})
                          .seqnum(4)
                          .build_state();

            // Channel
            auto chan = create_channel();
            chan.lowest_layer()
                .add_message(create_message(1, {0x01}))
                .add_message(create_coldef_message(2, protocol_field_type::var_string));
            chan.shared_sequence_number() = 42u;

            // Get an execution request into the channel's buffer
            concat(chan.shared_buffer(), {0x02, 0x05, 0x09});

            // Call the function
            fns.start_execution(chan, resultset_encoding::binary, st).validate_no_error();

            // We've written the request message
            auto expected_msg = create_message(0, {0x02, 0x05, 0x09});
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), expected_msg);
            BOOST_TEST(chan.shared_sequence_number() == 42u);  // unused

            // We've read the response
            BOOST_TEST(get_impl(st).encoding() == resultset_encoding::binary);
            BOOST_TEST(get_impl(st).sequence_number() == 3u);
            BOOST_TEST(st.should_read_rows());
            check_meta(
                get_impl(st).current_resultset_meta(),
                {std::make_pair(column_type::varchar, "mycol")}
            );
        }
    }
}

// This covers errors in both writing the request and calling read_resultset_head
BOOST_AUTO_TEST_CASE(error_network_error)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            for (std::size_t i = 0; i <= 1; ++i)
            {
                BOOST_TEST_CONTEXT(i)
                {
                    std::vector<field_view> fields;
                    auto st = exec_builder(false).reset(&fields).build_state();
                    auto chan = create_channel();
                    chan.lowest_layer()
                        .add_message(create_message(1, {0x01}))
                        .add_message(create_coldef_message(2, protocol_field_type::var_string))
                        .set_fail_count(fail_count(i, client_errc::server_unsupported));

                    // Get an execution request into the channel's buffer
                    concat(chan.shared_buffer(), {0x02, 0x05, 0x09});

                    // Call the function
                    fns.start_execution(chan, resultset_encoding::binary, st)
                        .validate_error_exact(client_errc::server_unsupported);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace