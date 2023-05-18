//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/results.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/results_impl.hpp>
#include <boost/mysql/detail/network_algorithms/execute_impl.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "assert_buffer_equals.hpp"
#include "buffer_concat.hpp"
#include "check_meta.hpp"
#include "creation/create_message.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_results.hpp"
#include "creation/create_row_message.hpp"
#include "mock_execution_processor.hpp"
#include "printing.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"
#include "test_stream.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::execution_processor;
using detail::protocol_field_type;
using detail::resultset_encoding;

namespace {

using netfun_maker = netfun_maker_fn<void, test_channel&, resultset_encoding, execution_processor&>;

struct
{
    typename netfun_maker::signature execute;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::execute_impl<test_stream>),           "sync" },
    {netfun_maker::async_errinfo(&detail::async_execute_impl<test_stream>), "async"}
};

BOOST_AUTO_TEST_SUITE(test_execute_impl)

BOOST_AUTO_TEST_CASE(eof)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            mock_execution_processor proc;
            auto ok_packet = ok_msg_builder().affected_rows(60u).info("abc").seqnum(1).build_ok();
            auto chan = create_channel(ok_packet);
            concat(chan.shared_buffer(), {0x02, 0x05, 0x09});  // some execution request

            // Call the function
            fns.execute(chan, resultset_encoding::binary, proc).validate_no_error();

            // We've written the exeuction request
            auto expected_msg = create_message(0, {0x02, 0x05, 0x09});
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), expected_msg);

            // We've read into the processor
            proc.num_calls().reset(1).on_head_ok_packet(1).validate();
            BOOST_TEST(proc.encoding() == resultset_encoding::binary);
            BOOST_TEST(proc.affected_rows() == 60u);
            BOOST_TEST(proc.info() == "abc");
            BOOST_TEST(chan.shared_sequence_number() == 0u);  // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(single_batch)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            mock_execution_processor proc;
            auto chan = create_channel();
            concat(chan.shared_buffer(), {0x02, 0x05, 0x09});  // some execution request
            std::vector<std::uint8_t> messages;
            concat(messages, create_message(1, {0x01}));                                // OK, 1 metadata
            concat(messages, create_coldef_message(2, protocol_field_type::longlong));  // meta
            concat(messages, create_text_row_message(3, 42));                           // row 1
            concat(messages, create_text_row_message(4, 43));                           // row 2
            concat(messages, ok_msg_builder().seqnum(5).affected_rows(10u).info("1st").build_eof());
            concat(messages, ok_msg_builder().seqnum(1).info("2nd").build_eof());  // don't read any further
            chan.lowest_layer().add_message(std::move(messages));

            // Call the function
            fns.execute(chan, resultset_encoding::text, proc).validate_no_error();

            // We've written the exeuction request
            auto expected_msg = create_message(0, {0x02, 0x05, 0x09});
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), expected_msg);

            // We've read the results
            proc.num_calls()
                .reset(1)
                .on_num_meta(1)
                .on_meta(1)
                .on_row_batch_start(1)
                .on_row(2)
                .on_row_batch_finish(1)
                .on_row_ok_packet(1)
                .validate();
            BOOST_TEST(proc.num_meta() == 1u);
            check_meta(proc.meta(), {column_type::bigint});
            BOOST_TEST(proc.affected_rows() == 10u);
            BOOST_TEST(proc.info() == "1st");
            BOOST_TEST(chan.shared_sequence_number() == 0u);  // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(multiple_batches)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            mock_execution_processor proc;
            auto chan = create_channel();
            concat(chan.shared_buffer(), {0x02, 0x05, 0x09});  // some execution request
            chan.lowest_layer()
                .add_message(create_message(1, {0x01}))                            // OK, 1 metadata
                .add_message(create_coldef_message(2, protocol_field_type::tiny))  // meta
                .add_message(create_text_row_message(3, 42))                       // row 1
                .add_message(create_text_row_message(4, 43))                       // row 2
                .add_message(ok_msg_builder().seqnum(5).affected_rows(10u).info("1st").build_eof());

            // Call the function
            fns.execute(chan, resultset_encoding::text, proc).validate_no_error();

            // We've written the exeuction request
            auto expected_msg = create_message(0, {0x02, 0x05, 0x09});
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), expected_msg);

            // We've read the results
            proc.num_calls()
                .reset(1)
                .on_num_meta(1)
                .on_meta(1)
                .on_row_batch_start(3)
                .on_row(2)
                .on_row_batch_finish(3)
                .on_row_ok_packet(1)
                .validate();
            BOOST_TEST(proc.num_meta() == 1u);
            check_meta(proc.meta(), {column_type::tinyint});
            BOOST_TEST(proc.affected_rows() == 10u);
            BOOST_TEST(proc.info() == "1st");
            BOOST_TEST(chan.shared_sequence_number() == 0u);  // not used
        }
    }
}

// Tests error on write, while reading head and while reading rows (error spotcheck)
BOOST_AUTO_TEST_CASE(error_network_error)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            for (std::size_t i = 0; i <= 2; ++i)
            {
                BOOST_TEST_CONTEXT("i=" << i)
                {
                    mock_execution_processor proc;
                    auto chan = create_channel();
                    concat(chan.shared_buffer(), {0x02, 0x05, 0x09});  // some execution request
                    chan.lowest_layer()
                        .add_message(concat_copy(
                            create_message(1, {0x01}),
                            create_coldef_message(2, protocol_field_type::tiny)
                        ))
                        .add_message(concat_copy(
                            create_text_row_message(3, 42),
                            ok_msg_builder().seqnum(4).info("1st").build_eof()
                        ))
                        .set_fail_count(fail_count(i, client_errc::wrong_num_params));

                    // Call the function
                    fns.execute(chan, resultset_encoding::text, proc)
                        .validate_error_exact(client_errc::wrong_num_params);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace