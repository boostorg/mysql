//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "creation/create_message_struct.hpp"
#include "printing.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using boost::mysql::column_type;
using boost::mysql::diagnostics;
using boost::mysql::error_code;
using boost::mysql::metadata;
using boost::mysql::metadata_mode;
using boost::mysql::string_view;

namespace {

class mock_execution_processor : public execution_processor
{
public:
    using execution_processor::execution_processor;
    using execution_processor::meta_mode;
    using execution_processor::set_state;
    using execution_processor::state_t;

    struct
    {
        metadata m;
        string_view column_name;
    } on_meta_impl_call{};

private:
    void reset_impl() noexcept override {}
    error_code on_head_ok_packet_impl(const ok_packet&, diagnostics&) override { return error_code(); }
    void on_num_meta_impl(std::size_t) override {}
    error_code on_meta_impl(metadata&& m, string_view column_name, diagnostics&) override
    {
        on_meta_impl_call.m = std::move(m);
        on_meta_impl_call.column_name = column_name;
        return error_code();
    }
    error_code on_row_ok_packet_impl(const ok_packet&) override { return error_code(); }
    error_code on_row_impl(deserialization_context, const output_ref&) override { return error_code(); }
    void on_row_batch_start_impl() override {}
    void on_row_batch_finish_impl() override {}
};

void check_reading_first(const execution_processor& st)
{
    BOOST_TEST(st.is_reading_first());
    BOOST_TEST(!st.is_reading_first_subseq());
    BOOST_TEST(st.is_reading_head());
    BOOST_TEST(!st.is_reading_meta());
    BOOST_TEST(!st.is_reading_rows());
    BOOST_TEST(!st.is_complete());
}

void check_reading_first_subseq(const execution_processor& st)
{
    BOOST_TEST(!st.is_reading_first());
    BOOST_TEST(st.is_reading_first_subseq());
    BOOST_TEST(st.is_reading_head());
    BOOST_TEST(!st.is_reading_meta());
    BOOST_TEST(!st.is_reading_rows());
    BOOST_TEST(!st.is_complete());
}

void check_reading_meta(const execution_processor& st)
{
    BOOST_TEST(!st.is_reading_first());
    BOOST_TEST(!st.is_reading_first_subseq());
    BOOST_TEST(!st.is_reading_head());
    BOOST_TEST(st.is_reading_meta());
    BOOST_TEST(!st.is_reading_rows());
    BOOST_TEST(!st.is_complete());
}

void check_reading_rows(const execution_processor& st)
{
    BOOST_TEST(!st.is_reading_first());
    BOOST_TEST(!st.is_reading_first_subseq());
    BOOST_TEST(!st.is_reading_head());
    BOOST_TEST(!st.is_reading_meta());
    BOOST_TEST(st.is_reading_rows());
    BOOST_TEST(!st.is_complete());
}

void check_complete(const execution_processor& st)
{
    BOOST_TEST(!st.is_reading_first());
    BOOST_TEST(!st.is_reading_first_subseq());
    BOOST_TEST(!st.is_reading_head());
    BOOST_TEST(!st.is_reading_meta());
    BOOST_TEST(!st.is_reading_rows());
    BOOST_TEST(st.is_complete());
}

BOOST_AUTO_TEST_SUITE(test_execution_processor)

BOOST_AUTO_TEST_CASE(default_ctor)
{
    mock_execution_processor p;
    check_reading_first(p);
    BOOST_TEST(p.encoding() == resultset_encoding::text);
    BOOST_TEST(p.sequence_number() == 0u);
    BOOST_TEST(p.meta_mode() == metadata_mode::minimal);
}

BOOST_AUTO_TEST_CASE(reset)
{
    mock_execution_processor p;
    p.set_state(mock_execution_processor::state_t::reading_metadata);
    p.sequence_number() = 42u;
    p.reset(resultset_encoding::binary, metadata_mode::full);
    check_reading_first(p);
    BOOST_TEST(p.encoding() == resultset_encoding::binary);
    BOOST_TEST(p.sequence_number() == 0u);
    BOOST_TEST(p.meta_mode() == metadata_mode::full);
}

BOOST_AUTO_TEST_CASE(states)
{
    mock_execution_processor p;
    check_reading_first(p);

    p.set_state(mock_execution_processor::state_t::reading_metadata);
    check_reading_meta(p);

    p.set_state(mock_execution_processor::state_t::reading_rows);
    check_reading_rows(p);

    p.set_state(mock_execution_processor::state_t::reading_first_subseq);
    check_reading_first_subseq(p);

    p.set_state(mock_execution_processor::state_t::reading_metadata);
    check_reading_meta(p);

    p.set_state(mock_execution_processor::state_t::reading_rows);
    check_reading_rows(p);

    p.set_state(mock_execution_processor::state_t::complete);
    check_complete(p);
}

BOOST_AUTO_TEST_CASE(on_meta_mode_minimal)
{
    mock_execution_processor p;
    diagnostics diag;

    p.reset(resultset_encoding::text, metadata_mode::minimal);
    p.set_state(mock_execution_processor::state_t::reading_metadata);
    p.on_meta(create_coldef(protocol_field_type::bit, "myname"), diag);

    // Metadata object didn't copy the strings, and the column_name argument got the right thing
    BOOST_TEST(p.on_meta_impl_call.m.type() == column_type::bit);
    BOOST_TEST(p.on_meta_impl_call.m.column_name() == "");
    BOOST_TEST(p.on_meta_impl_call.column_name == "myname");
}

BOOST_AUTO_TEST_CASE(on_meta_mode_full)
{
    mock_execution_processor p;
    diagnostics diag;

    p.reset(resultset_encoding::text, metadata_mode::full);
    p.set_state(mock_execution_processor::state_t::reading_metadata);
    p.on_meta(create_coldef(protocol_field_type::bit, "myname"), diag);

    // Metadata object copied the strings and the column_name argument got the right thing
    BOOST_TEST(p.on_meta_impl_call.m.type() == column_type::bit);
    BOOST_TEST(p.on_meta_impl_call.m.column_name() == "myname");
    BOOST_TEST(p.on_meta_impl_call.column_name == "myname");
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace