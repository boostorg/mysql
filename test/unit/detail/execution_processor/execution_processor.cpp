//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/test/unit_test.hpp>

#include "printing.hpp"

using namespace boost::mysql::detail;
using boost::mysql::diagnostics;
using boost::mysql::error_code;
using boost::mysql::metadata_mode;

namespace {

class mock_execution_processor : public execution_processor
{
public:
    using execution_processor::execution_processor;
    using execution_processor::meta_mode;
    using execution_processor::set_state;
    using execution_processor::state_t;

    virtual void reset_impl() noexcept {}
    virtual error_code on_head_ok_packet_impl(const ok_packet&, diagnostics&) { return error_code(); }
    virtual void on_num_meta_impl(std::size_t) {}
    virtual error_code on_meta_impl(const column_definition_packet&, diagnostics&) { return error_code(); }
    virtual error_code on_row_ok_packet_impl(const ok_packet&) { return error_code(); }
    virtual error_code on_row_impl(deserialization_context&) { return error_code(); }
    virtual void on_row_batch_start_impl() {}
    virtual void on_row_batch_finish_impl() {}
    virtual std::size_t num_meta_impl() const noexcept { return 0; }
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

BOOST_AUTO_TEST_SUITE_END()

}  // namespace