//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/blob.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/detail/network_algorithms/start_execution_impl.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "assert_buffer_equals.hpp"
#include "check_meta.hpp"
#include "creation/create_message.hpp"
#include "mock_execution_processor.hpp"
#include "printing.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::mysql::detail::execution_processor;
using boost::mysql::detail::protocol_field_type;
using boost::mysql::detail::resultset_encoding;

namespace {

using netfun_maker = netfun_maker_fn<void, test_channel&, resultset_encoding, execution_processor&>;

struct
{
    typename netfun_maker::signature start_execution;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&boost::mysql::detail::start_execution_impl),           "sync" },
    {netfun_maker::async_errinfo(&boost::mysql::detail::async_start_execution_impl), "async"}
};

BOOST_AUTO_TEST_SUITE(test_start_execution_impl)

BOOST_AUTO_TEST_CASE(success)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            mock_execution_processor st;

            // Channel
            auto chan = create_channel();
            chan.set_meta_mode(metadata_mode::full);
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
            BOOST_TEST(st.encoding() == resultset_encoding::binary);
            BOOST_TEST(st.sequence_number() == 3u);
            BOOST_TEST(st.is_reading_rows());
            check_meta(st.meta(), {std::make_pair(column_type::varchar, "mycol")});

            // Validate mock calls
            st.num_calls().reset(1).on_num_meta(1).on_meta(1).validate();
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
                    mock_execution_processor st;

                    // Channel
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

                    // Num calls validation
                    st.num_calls().reset(1).validate();
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace