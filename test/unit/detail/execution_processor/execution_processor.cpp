//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "creation/create_message_struct.hpp"
#include "creation/create_meta.hpp"
#include "mock_execution_processor.hpp"
#include "printing.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using boost::mysql::column_type;
using boost::mysql::diagnostics;
using boost::mysql::error_code;
using boost::mysql::metadata;
using boost::mysql::metadata_mode;
using boost::mysql::string_view;
using boost::mysql::throw_on_error;

namespace {

class spy_execution_processor : public mock_execution_processor
{
public:
    struct
    {
        metadata meta;
        std::string column_name;
        bool is_last;
    } on_meta_call;

private:
    error_code on_meta_impl(metadata&& m, string_view column_name, bool is_last, diagnostics& diag) override
    {
        on_meta_call.meta = m;
        on_meta_call.column_name = column_name;
        on_meta_call.is_last = is_last;
        return mock_execution_processor::on_meta_impl(std::move(m), column_name, is_last, diag);
    }
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
    p.on_num_meta(42);
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
    diagnostics diag;

    check_reading_first(p);

    p.on_num_meta(1);
    check_reading_meta(p);

    auto err = p.on_meta(meta_builder().build(), diag);
    throw_on_error(err, diag);
    check_reading_rows(p);

    err = p.on_row_ok_packet(ok_builder().more_results(true).build());
    throw_on_error(err, diag);
    check_reading_first_subseq(p);

    err = p.on_head_ok_packet(ok_builder().build(), diag);
    throw_on_error(err, diag);
    check_complete(p);
}

BOOST_AUTO_TEST_CASE(on_meta_mode_minimal)
{
    spy_execution_processor p;
    diagnostics diag;
    p.reset(resultset_encoding::text, metadata_mode::minimal);
    p.on_num_meta(1);

    auto err = p.on_meta(create_coldef(protocol_field_type::bit, "myname"), diag);

    // Metadata object shouldn't copy the strings, and the other args get the right thing
    BOOST_TEST(err == error_code());
    p.num_calls().reset(1).on_num_meta(1).on_meta(1).validate();
    BOOST_TEST(p.on_meta_call.meta.type() == column_type::bit);
    BOOST_TEST(p.on_meta_call.meta.column_name() == "");
    BOOST_TEST(p.on_meta_call.column_name == "myname");
    BOOST_TEST(p.on_meta_call.is_last);
}

BOOST_AUTO_TEST_CASE(on_meta_mode_full)
{
    spy_execution_processor p;
    diagnostics diag;
    p.reset(resultset_encoding::text, metadata_mode::full);
    p.on_num_meta(2);

    auto err = p.on_meta(create_coldef(protocol_field_type::bit, "myname"), diag);

    // Metadata object should copy the strings, and the other args get the right thing
    BOOST_TEST(err == error_code());
    p.num_calls().reset(1).on_num_meta(1).on_meta(1).validate();
    BOOST_TEST(p.on_meta_call.meta.type() == column_type::bit);
    BOOST_TEST(p.on_meta_call.meta.column_name() == "myname");
    BOOST_TEST(p.on_meta_call.column_name == "myname");
    BOOST_TEST(!p.on_meta_call.is_last);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace