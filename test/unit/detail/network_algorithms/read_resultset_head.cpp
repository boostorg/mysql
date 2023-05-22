//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/static_execution_state.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/protocol/common_messages.hpp>

#include <boost/test/unit_test.hpp>

#include "buffer_concat.hpp"
#include "check_meta.hpp"
#include "creation/create_diagnostics.hpp"
#include "creation/create_execution_state.hpp"
#include "creation/create_message.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_meta.hpp"
#include "creation/create_row_message.hpp"
#include "mock_execution_processor.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"
#include "test_stream.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;
using boost::mysql::detail::execution_processor;
using boost::mysql::detail::protocol_field_type;

namespace {

BOOST_AUTO_TEST_SUITE(test_read_resultset_head)

BOOST_AUTO_TEST_SUITE(detail_)  // tests the overload that can be passed an execution_processor

using netfun_maker = netfun_maker_fn<void, test_channel&, execution_processor&>;

struct
{
    typename netfun_maker::signature read_resultset_head;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::read_resultset_head),           "sync_errc"    },
    {netfun_maker::async_errinfo(&detail::async_read_resultset_head), "async_errinfo"},
};

struct fixture
{
    test_channel chan{create_channel()};
    mock_execution_processor st;

    fixture()
    {
        // The initial request writing should have advanced this to 1 (or bigger)
        st.sequence_number() = 1;
    }
};

BOOST_AUTO_TEST_CASE(success_meta)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_message(1, {0x01});
            auto col = create_coldef_message(2, protocol_field_type::var_string);
            fix.chan.lowest_layer().add_message(concat_copy(response, col));

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // We've read the response
            fix.st.num_calls().on_num_meta(1).on_meta(1).validate();
            BOOST_TEST(fix.st.is_reading_rows());
            BOOST_TEST(fix.st.sequence_number() == 3u);
            BOOST_TEST(fix.st.num_meta() == 1u);
            check_meta(fix.st.meta(), {std::make_pair(column_type::varchar, "mycol")});
        }
    }
}

BOOST_AUTO_TEST_CASE(success_several_meta_separate)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_message(1, {0x02});
            auto col1 = create_coldef_message(2, protocol_field_type::var_string, "f1");
            auto col2 = create_coldef_message(3, protocol_field_type::tiny, "f2");
            fix.chan.lowest_layer().add_message(concat_copy(response, col1));
            fix.chan.lowest_layer().add_message(col2);

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // We've read the response
            fix.st.num_calls().on_num_meta(1).on_meta(2).validate();
            BOOST_TEST(fix.st.is_reading_rows());
            BOOST_TEST(fix.st.sequence_number() == 4u);
            BOOST_TEST(fix.st.num_meta() == 2u);
            check_meta(
                fix.st.meta(),
                {
                    std::make_pair(column_type::varchar, "f1"),
                    std::make_pair(column_type::tinyint, "f2"),
                }
            );
        }
    }
}

BOOST_AUTO_TEST_CASE(success_ok_packet)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = ok_msg_builder().seqnum(1).affected_rows(42).info("abc").build_ok();
            fix.chan.lowest_layer().add_message(response);

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // We've read the response
            fix.st.num_calls().on_head_ok_packet(1).validate();
            BOOST_TEST(fix.st.meta().size() == 0u);
            BOOST_TEST(fix.st.is_complete());
            BOOST_TEST(fix.st.affected_rows() == 42u);
            BOOST_TEST(fix.st.info() == "abc");
        }
    }
}

// Check that we don't attempt to read the rows even if they're available
BOOST_AUTO_TEST_CASE(success_rows_available)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_message(1, {0x01});
            auto col1 = create_coldef_message(2, protocol_field_type::var_string, "f1");
            auto row1 = create_text_row_message(3, "abc");
            fix.chan.lowest_layer().add_message(concat_copy(response, col1, row1));

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // We've read the response but not the rows
            fix.st.num_calls().on_num_meta(1).on_meta(1).validate();
            BOOST_TEST(fix.st.is_reading_rows());
            BOOST_TEST(fix.st.sequence_number() == 3u);
        }
    }
}

// Check that we don't attempt to read the next resultset even if it's available
BOOST_AUTO_TEST_CASE(success_ok_packet_next_resultset)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto ok1 = ok_msg_builder().seqnum(1).info("1st").more_results(true).build_ok();
            auto ok2 = ok_msg_builder().seqnum(2).info("2nd").build_ok();
            fix.chan.lowest_layer().add_message(concat_copy(ok1, ok2));

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // We've read the response
            fix.st.num_calls().on_head_ok_packet(1).validate();
            BOOST_TEST(fix.st.is_reading_first_subseq());
            BOOST_TEST(fix.st.info() == "1st");
        }
    }
}

// Should be a no-op
BOOST_AUTO_TEST_CASE(state_complete)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            add_ok(fix.st, ok_builder().affected_rows(42).build());

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // Nothing changed
            fix.st.num_calls().on_head_ok_packet(1).validate();
            BOOST_TEST(fix.st.is_complete());
            BOOST_TEST(fix.st.affected_rows() == 42u);
        }
    }
}

// Should be a no-op
BOOST_AUTO_TEST_CASE(state_reading_rows)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            add_meta(fix.st, {meta_builder().type(column_type::bit).build()});

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_no_error();

            // Nothing changed
            fix.st.num_calls().on_num_meta(1).on_meta(1).validate();
            BOOST_TEST(fix.st.is_reading_rows());
            check_meta(fix.st.meta(), {column_type::bit});
        }
    }
}

BOOST_AUTO_TEST_CASE(error_network_error)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            // This covers: error reading the initial response and
            // error reading successive metadata packets
            for (std::size_t i = 0; i <= 2; ++i)
            {
                BOOST_TEST_CONTEXT(i)
                {
                    fixture fix;
                    auto response = create_message(1, {0x02});
                    auto col1 = create_coldef_message(2, protocol_field_type::var_string, "f1");
                    auto col2 = create_coldef_message(3, protocol_field_type::tiny, "f2");
                    fix.chan.lowest_layer().add_message(response);
                    fix.chan.lowest_layer().add_message(col1);
                    fix.chan.lowest_layer().add_message(col2);
                    fix.chan.lowest_layer().set_fail_count(fail_count(i, client_errc::server_unsupported));

                    // Call the function
                    fns.read_resultset_head(fix.chan, fix.st)
                        .validate_error_exact(client_errc::server_unsupported);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(error_metadata_packets_seqnum_mismatch)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_message(1, {0x02});
            auto col1 = create_coldef_message(2, protocol_field_type::var_string, "f1");
            auto col2 = create_coldef_message(4, protocol_field_type::tiny, "f2");
            fix.chan.lowest_layer().add_message(response);
            fix.chan.lowest_layer().add_message(col1);
            fix.chan.lowest_layer().add_message(col2);

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st)
                .validate_error_exact(client_errc::sequence_number_mismatch);
        }
    }
}

// All cases where the deserialization of the execution_response
// yields an error are handled uniformly, so it's enough with this test
BOOST_AUTO_TEST_CASE(error_deserialize_execution_response)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_err_packet_message(1, common_server_errc::er_bad_db_error, "no_db");
            fix.chan.lowest_layer().add_message(response);

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st)
                .validate_error_exact(common_server_errc::er_bad_db_error, "no_db");
        }
    }
}

BOOST_AUTO_TEST_CASE(error_deserialize_metadata)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            auto response = create_message(1, {0x01});
            auto col = create_message(2, {0x08, 0x03});
            fix.chan.lowest_layer().add_message(response);
            fix.chan.lowest_layer().add_message(col);

            // Call the function
            fns.read_resultset_head(fix.chan, fix.st).validate_error_exact(client_errc::incomplete_message);
        }
    }
}

// The execution processor signals an error on head packet (e.g. meta mismatch)
BOOST_AUTO_TEST_CASE(error_on_head_ok_packet)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            fix.st.set_fail_count(
                fail_count(0, client_errc::metadata_check_failed),
                create_client_diag("some message")
            );

            auto response = ok_msg_builder().seqnum(1).affected_rows(42).info("abc").build_ok();
            fix.chan.lowest_layer().add_message(response);

            fns.read_resultset_head(fix.chan, fix.st)
                .validate_error_exact_client(client_errc::metadata_check_failed, "some message");
            fix.st.num_calls().on_head_ok_packet(1).validate();
        }
    }
}

BOOST_AUTO_TEST_CASE(error_on_meta)
{
    for (auto fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            fix.st.set_fail_count(
                fail_count(0, client_errc::metadata_check_failed),
                create_client_diag("some message")
            );

            auto response = create_message(1, {0x01});
            auto col = create_coldef_message(2, protocol_field_type::var_string);
            fix.chan.lowest_layer().add_message(concat_copy(response, col));

            fns.read_resultset_head(fix.chan, fix.st)
                .validate_error_exact_client(client_errc::metadata_check_failed, "some message");
            fix.st.num_calls().on_num_meta(1).on_meta(1).validate();
        }
    }
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}  // namespace