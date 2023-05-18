//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/config.hpp>

#ifdef BOOST_MYSQL_CXX14

#include <boost/mysql/common_server_errc.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows_impl.hpp>

#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>

#include "creation/create_execution_processor.hpp"
#include "creation/create_message.hpp"
#include "creation/create_message_struct.hpp"
#include "creation/create_meta.hpp"
#include "creation/create_row_message.hpp"
#include "mock_execution_processor.hpp"
#include "test_channel.hpp"
#include "test_common.hpp"
#include "test_stream.hpp"
#include "unit_netfun_maker.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;
using boost::span;
using boost::mysql::detail::execution_processor;
using boost::mysql::detail::output_ref;

namespace {

BOOST_AUTO_TEST_SUITE(test_read_some_rows_impl)

using row1 = std::tuple<int>;

using netfun_maker = netfun_maker_fn<std::size_t, test_channel&, execution_processor&, const output_ref&>;

struct
{
    typename netfun_maker::signature read_some_rows_impl;
    const char* name;
} all_fns[] = {
    {netfun_maker::sync_errc(&detail::read_some_rows_impl),           "sync" },
    {netfun_maker::async_errinfo(&detail::async_read_some_rows_impl), "async"},
};

struct fixture
{
    mock_execution_processor proc;
    test_channel chan{create_channel()};
    std::array<row1, 3> storage;

    fixture()
    {
        // Prepare the processor, such that it's ready to read rows
        add_meta(proc, {meta_builder().type(column_type::varchar).name("fvarchar").nullable(false).build()});
        proc.sequence_number() = 42;
    }

    void validate_refs(std::size_t num_rows)
    {
        BOOST_TEST_REQUIRE(proc.refs().size() == num_rows);
        for (std::size_t i = 0; i < num_rows; ++i)
            BOOST_TEST(proc.refs()[i].offset() == i);
    }

    output_ref ref() noexcept { return output_ref(span<row1>(storage), 0); }
};

BOOST_AUTO_TEST_CASE(eof)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            fix.chan.lowest_layer().add_message(
                ok_msg_builder().affected_rows(1).info("1st").seqnum(42).more_results(true).build_eof()
            );

            std::size_t num_rows = fns.read_some_rows_impl(fix.chan, fix.proc, fix.ref()).get();
            BOOST_TEST(num_rows == 0u);
            BOOST_TEST(fix.proc.is_reading_head());
            BOOST_TEST(fix.proc.affected_rows() == 1u);
            BOOST_TEST(fix.proc.info() == "1st");
            BOOST_TEST(fix.chan.shared_sequence_number() == 0u);  // not used
            fix.proc.num_calls()
                .on_num_meta(1)
                .on_meta(1)
                .on_row_batch_start(1)
                .on_row_ok_packet(1)
                .on_row_batch_finish(1)
                .validate();
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

            std::size_t num_rows = fns.read_some_rows_impl(fix.chan, fix.proc, fix.ref()).get();
            BOOST_TEST(num_rows == 2u);
            BOOST_TEST(fix.proc.is_reading_rows());
            BOOST_TEST(fix.chan.shared_sequence_number() == 0u);  // not used
            fix.validate_refs(2);
            fix.proc.num_calls()
                .on_num_meta(1)
                .on_meta(1)
                .on_row_batch_start(1)
                .on_row(2)
                .on_row_batch_finish(1)
                .validate();
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
                ok_msg_builder().seqnum(44).affected_rows(1).info("1st").more_results(true).build_eof()
            ));

            std::size_t num_rows = fns.read_some_rows_impl(fix.chan, fix.proc, fix.ref()).get();
            BOOST_TEST(num_rows == 2u);
            BOOST_TEST_REQUIRE(fix.proc.is_reading_head());
            BOOST_TEST(fix.proc.affected_rows() == 1u);
            BOOST_TEST(fix.proc.info() == "1st");
            BOOST_TEST(fix.chan.shared_sequence_number() == 0u);  // not used
            fix.validate_refs(2);
            fix.proc.num_calls()
                .on_num_meta(1)
                .on_meta(1)
                .on_row_batch_start(1)
                .on_row(2)
                .on_row_ok_packet(1)
                .on_row_batch_finish(1)
                .validate();
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
            fix.chan.lowest_layer().add_message(concat_copy(
                create_text_row_message(42, "abc"),
                ok_msg_builder().seqnum(43).affected_rows(1).info("1st").more_results(true).build_eof(),
                ok_msg_builder().seqnum(44).info("2nd").build_ok()
            ));

            std::size_t num_rows = fns.read_some_rows_impl(fix.chan, fix.proc, fix.ref()).get();
            BOOST_TEST(num_rows == 1u);
            BOOST_TEST_REQUIRE(fix.proc.is_reading_head());
            BOOST_TEST(fix.proc.affected_rows() == 1u);
            BOOST_TEST(fix.proc.info() == "1st");
            fix.validate_refs(1);
            fix.proc.num_calls()
                .on_num_meta(1)
                .on_meta(1)
                .on_row_batch_start(1)
                .on_row(1)
                .on_row_ok_packet(1)
                .on_row_batch_finish(1)
                .validate();
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
            fix.chan.lowest_layer().add_message(concat_copy(
                create_text_row_message(42, "aaa"),
                create_text_row_message(43, "bbb"),
                create_text_row_message(44, "ccc"),
                create_text_row_message(45, "ddd")
            ));

            // We only have space for 3
            std::size_t num_rows = fns.read_some_rows_impl(fix.chan, fix.proc, fix.ref()).get();
            BOOST_TEST(num_rows == 3u);
            fix.validate_refs(3);
            BOOST_TEST(fix.proc.is_reading_rows());
            fix.proc.num_calls()
                .on_num_meta(1)
                .on_meta(1)
                .on_row_batch_start(1)
                .on_row(3)
                .on_row_batch_finish(1)
                .validate();
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
            add_ok(fix.proc, ok_builder().affected_rows(20).build());

            std::size_t num_rows = fns.read_some_rows_impl(fix.chan, fix.proc, fix.ref()).get();
            BOOST_TEST(num_rows == 0u);
            BOOST_TEST(fix.proc.is_complete());
            fix.proc.num_calls().on_num_meta(1).on_meta(1).on_row_ok_packet(1).validate();
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
            add_ok(fix.proc, ok_builder().affected_rows(42).more_results(true).build());

            std::size_t num_rows = fns.read_some_rows_impl(fix.chan, fix.proc, fix.ref()).get();
            BOOST_TEST(num_rows == 0u);
            BOOST_TEST(fix.proc.is_reading_head());
            fix.proc.num_calls().on_num_meta(1).on_meta(1).on_row_ok_packet(1).validate();
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
                    fix.chan.lowest_layer()
                        .add_message(create_text_row_message(42, "abc"))
                        .add_message(ok_msg_builder().seqnum(43).affected_rows(1).info("1st").build_eof())
                        .set_fail_count(fail_count(i, client_errc::wrong_num_params));

                    fns.read_some_rows_impl(fix.chan, fix.proc, fix.ref())
                        .validate_error_exact(client_errc::wrong_num_params);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(error_on_row)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            fix.chan.lowest_layer().add_message(create_text_row_message(42, 10));

            // Mock a failure
            fix.proc.set_fail_count(fail_count(0, client_errc::static_row_parsing_error));

            // Call the function
            fns.read_some_rows_impl(fix.chan, fix.proc, fix.ref())
                .validate_error_exact(client_errc::static_row_parsing_error);
        }
    }
}

BOOST_AUTO_TEST_CASE(error_on_row_ok_packet)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            fix.chan.lowest_layer().add_message(ok_msg_builder().seqnum(42).build_eof());

            // Mock a failure
            fix.proc.set_fail_count(fail_count(0, client_errc::num_resultsets_mismatch));

            // Call the function
            fns.read_some_rows_impl(fix.chan, fix.proc, fix.ref())
                .validate_error_exact(client_errc::num_resultsets_mismatch);
        }
    }
}

// deserialize_row_message covers cases like getting an error packet, seqnum mismatch, etc
BOOST_AUTO_TEST_CASE(error_deserialize_row_message)
{
    for (const auto& fns : all_fns)
    {
        BOOST_TEST_CONTEXT(fns.name)
        {
            fixture fix;
            fix.chan.lowest_layer().add_message(
                create_err_packet_message(42, common_server_errc::er_cant_create_db)
            );

            // Call the function
            fns.read_some_rows_impl(fix.chan, fix.proc, fix.ref())
                .validate_error_exact(common_server_errc::er_cant_create_db);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace

#endif