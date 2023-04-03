//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/execution_state.hpp>

#include <boost/mysql/detail/network_algorithms/execute_impl.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "assert_buffer_equals.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_row_message.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"
#include "test_stream.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using detail::protocol_field_type;
using detail::resultset_encoding;

namespace {

using netfun_maker = netfun_maker_fn<void, test_channel&, resultset_encoding, results&>;

struct
{
    typename netfun_maker::signature execute;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::execute_impl<test_stream>),           "sync" },
    {netfun_maker::async_errinfo(&detail::async_execute_impl<test_stream>), "async"}
};

// Verify that we clear any previous result
results create_initial_results()
{
    return create_results({
        {{protocol_field_type::bit, protocol_field_type::var_string},
         makerows(2, 30, "abc", 40, "bhj"),
         ok_builder().affected_rows(89).info("abc").build()}
    });
}

BOOST_AUTO_TEST_SUITE(test_execute)

BOOST_AUTO_TEST_CASE(empty_resultset)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto result = create_initial_results();
            auto ok_packet = ok_msg_builder().affected_rows(60u).info("abc").seqnum(1).build_ok();
            auto chan = create_channel(ok_packet);
            concat(chan.shared_buffer(), {0x02, 0x05, 0x09});  // some execution request

            // Call the function
            fns.execute(chan, resultset_encoding::binary, result).validate_no_error();

            // We've written the exeuction request
            auto expected_msg = create_message(0, {0x02, 0x05, 0x09});
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), expected_msg);

            // We've populated the results
            BOOST_TEST_REQUIRE(result.size() == 1u);
            BOOST_TEST(result[0].affected_rows() == 60u);
            BOOST_TEST(result[0].info() == "abc");
            BOOST_TEST(result[0].rows() == rows());
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
            auto result = create_initial_results();
            auto chan = create_channel();
            concat(chan.shared_buffer(), {0x02, 0x05, 0x09});  // some execution request
            std::vector<std::uint8_t> messages;
            concat(messages, create_message(1, {0x01}));                                // OK, 1 metadata
            concat(messages, create_coldef_message(2, protocol_field_type::longlong));  // meta
            concat(messages, create_text_row_message(3, 42));                           // row 1
            concat(messages, create_text_row_message(4, 43));                           // row 2
            concat(
                messages,
                ok_msg_builder().seqnum(5).affected_rows(10u).info("1st").more_results(true).build_eof()
            );
            concat(
                messages,
                ok_msg_builder().seqnum(6).affected_rows(20u).info("2nd").more_results(true).build_ok()
            );
            concat(messages, create_message(7, {0x01}));                                  // OK, 1 metadata
            concat(messages, create_coldef_message(8, protocol_field_type::var_string));  // meta
            concat(messages, create_text_row_message(9, "abc"));                          // row 1
            concat(messages, ok_msg_builder().seqnum(10).affected_rows(30u).info("3rd").build_eof());
            chan.lowest_layer().add_message(std::move(messages));

            // Call the function
            fns.execute(chan, resultset_encoding::text, result).validate_no_error();

            // We've written the exeuction request
            auto expected_msg = create_message(0, {0x02, 0x05, 0x09});
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), expected_msg);

            // We've populated the results
            BOOST_TEST_REQUIRE(result.size() == 3u);
            BOOST_TEST(result[0].affected_rows() == 10u);
            BOOST_TEST(result[0].info() == "1st");
            BOOST_TEST(result[0].rows() == makerows(1, 42, 43));
            BOOST_TEST(result[1].affected_rows() == 20u);
            BOOST_TEST(result[1].info() == "2nd");
            BOOST_TEST(result[1].rows() == rows());
            BOOST_TEST(result[2].affected_rows() == 30u);
            BOOST_TEST(result[2].info() == "3rd");
            BOOST_TEST(result[2].rows() == makerows(1, "abc"));
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
            auto result = create_initial_results();
            auto chan = create_channel();
            concat(chan.shared_buffer(), {0x02, 0x05, 0x09});  // some execution request
            chan.lowest_layer()
                .add_message(create_message(1, {0x01}))                            // OK, 1 metadata
                .add_message(create_coldef_message(2, protocol_field_type::tiny))  // meta
                .add_message(create_text_row_message(3, 42))                       // row 1
                .add_message(create_text_row_message(4, 43))                       // row 2
                .add_message(
                    ok_msg_builder().seqnum(5).affected_rows(10u).info("1st").more_results(true).build_eof()
                )
                .add_message(
                    ok_msg_builder().seqnum(6).affected_rows(20u).info("2nd").more_results(true).build_ok()
                )
                .add_message(create_message(7, {0x01}))                                  // OK, 1 metadata
                .add_message(create_coldef_message(8, protocol_field_type::var_string))  // meta
                .add_message(create_text_row_message(9, "ab"))                           // row 1
                .add_message(ok_msg_builder().seqnum(10).affected_rows(30u).info("3rd").build_eof());

            // Call the function
            fns.execute(chan, resultset_encoding::text, result).validate_no_error();

            // We've written the exeuction request
            auto expected_msg = create_message(0, {0x02, 0x05, 0x09});
            BOOST_MYSQL_ASSERT_BLOB_EQUALS(chan.lowest_layer().bytes_written(), expected_msg);

            // We've populated the results
            BOOST_TEST_REQUIRE(result.size() == 3u);
            BOOST_TEST(result[0].affected_rows() == 10u);
            BOOST_TEST(result[0].info() == "1st");
            BOOST_TEST(result[0].rows() == makerows(1, 42, 43));
            BOOST_TEST(result[1].affected_rows() == 20u);
            BOOST_TEST(result[1].info() == "2nd");
            BOOST_TEST(result[1].rows() == rows());
            BOOST_TEST(result[2].affected_rows() == 30u);
            BOOST_TEST(result[2].info() == "3rd");
            BOOST_TEST(result[2].rows() == makerows(1, "ab"));
            BOOST_TEST(chan.shared_sequence_number() == 0u);  // not used
        }
    }
}

BOOST_AUTO_TEST_CASE(error_network_error)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // Tests error on write, while reading head and while reading rows
            for (std::size_t i = 0; i <= 2; ++i)
            {
                BOOST_TEST_CONTEXT("i=" << i)
                {
                    auto result = create_initial_results();
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
                    fns.execute(chan, resultset_encoding::text, result)
                        .validate_error_exact(client_errc::wrong_num_params);
                }
            }
        }
    }
}

// Seqnum mismatch on row messages
BOOST_AUTO_TEST_CASE(error_seqnum_mismatch)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto result = create_initial_results();
            auto chan = create_channel();
            concat(chan.shared_buffer(), {0x02, 0x05, 0x09});  // some execution request
            chan.lowest_layer().add_message(concat_copy(
                create_message(1, {0x01}),
                create_coldef_message(2, protocol_field_type::tiny),
                create_text_row_message(3, 42),
                ok_msg_builder().seqnum(0).info("1st").build_eof()
            ));

            // Call the function
            fns.execute(chan, resultset_encoding::text, result)
                .validate_error_exact(client_errc::sequence_number_mismatch);
        }
    }
}

BOOST_AUTO_TEST_CASE(error_deserializing_rows)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            auto result = create_initial_results();
            auto chan = create_channel();
            concat(chan.shared_buffer(), {0x02, 0x05, 0x09});  // some execution request
            chan.lowest_layer().add_message(concat_copy(
                create_message(1, {0x01}),
                create_coldef_message(2, protocol_field_type::tiny),
                create_message(3, {0x02, 0xff})  // bad row
            ));

            // Call the function
            fns.execute(chan, resultset_encoding::text, result)
                .validate_error_exact(client_errc::incomplete_message);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace