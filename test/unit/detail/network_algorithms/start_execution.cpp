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
using boost::mysql::detail::execution_state_impl;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::resultset_encoding;
using namespace boost::mysql::test;

namespace {

using netfun_maker = netfun_maker_fn<
    void,
    test_channel&,
    error_code,
    resultset_encoding,
    execution_state_impl&>;

struct
{
    typename netfun_maker::signature start_execution;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&boost::mysql::detail::start_execution),           "sync" },
    {netfun_maker::async_errinfo(&boost::mysql::detail::async_start_execution), "async"}
};

// Verify that we reset the state
execution_state_impl create_initial_state()
{
    return exec_builder(false, resultset_encoding::text)
        .meta({protocol_field_type::geometry})
        .seqnum(4)
        .build();
}

BOOST_AUTO_TEST_SUITE(test_start_execution)

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto chan = create_channel();
            chan.lowest_layer()
                .add_message(create_message(1, {0x01}))
                .add_message(create_coldef_message(2, protocol_field_type::var_string));
            chan.shared_sequence_number() = 42u;
            auto st = create_initial_state();

            // Get an execution request into the channel's buffer
            concat(chan.shared_buffer(), {0x02, 0x05, 0x09});

            // Call the function
            fns.start_execution(chan, error_code(), resultset_encoding::binary, st).validate_no_error();

            // We've written the request message
            auto expected_msg = create_message(0, {0x02, 0x05, 0x09});
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), expected_msg);
            BOOST_TEST(chan.shared_sequence_number() == 42u);  // unused

            // We've read the response
            BOOST_TEST(st.encoding() == resultset_encoding::binary);
            BOOST_TEST(st.sequence_number() == 3u);
            BOOST_TEST(st.should_read_rows());
            check_meta(st.current_resultset_meta(), {std::make_pair(column_type::varchar, "mycol")});
        }
    }
}

BOOST_AUTO_TEST_CASE(error_fast_fail)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto chan = create_channel(ok_msg_builder().build_ok());
            auto st = create_initial_state();

            // Get an execution request into the channel's buffer
            concat(chan.shared_buffer(), {0x02, 0x05, 0x09});

            // Call the function
            fns.start_execution(chan, client_errc::wrong_num_params, resultset_encoding::binary, st)
                .validate_error_exact(client_errc::wrong_num_params);

            // We didn't write the message
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), blob());
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
                    auto chan = create_channel();
                    chan.lowest_layer()
                        .add_message(create_message(1, {0x01}))
                        .add_message(create_coldef_message(2, protocol_field_type::var_string))
                        .set_fail_count(fail_count(i, client_errc::server_unsupported));
                    auto st = create_initial_state();

                    // Get an execution request into the channel's buffer
                    concat(chan.shared_buffer(), {0x02, 0x05, 0x09});

                    // Call the function
                    fns.start_execution(chan, error_code(), resultset_encoding::binary, st)
                        .validate_error_exact(client_errc::server_unsupported);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace