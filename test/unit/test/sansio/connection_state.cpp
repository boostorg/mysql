//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state.hpp>

#include <boost/test/unit_test.hpp>

#include <array>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/printing.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

BOOST_AUTO_TEST_SUITE(test_connection_state)

namespace {

void prepare_busy_state(detail::connection_state& st)
{
    st.data().shared_pipeline_stages = {
        detail::pipeline_request_stage{detail::pipeline_stage_kind::execute, 42u, detail::resultset_encoding::text},
        detail::pipeline_request_stage{}};
    st.data().op_in_progress = true;
}

template <class AlgoParams>
void check_no_modification(AlgoParams params)
{
    detail::connection_state st(16, 1024, false);
    diagnostics diag;
    prepare_busy_state(st);

    const auto before_stages = st.data().shared_pipeline_stages;

    auto res = st.setup(diag, params);
    auto act = res.resume(error_code(), 0);

    BOOST_TEST(act.is_done());
    BOOST_TEST(act.error() == client_errc::operation_in_progress);
    const auto after_stages = st.data().shared_pipeline_stages;

    BOOST_TEST(after_stages == before_stages);
}

}  // namespace

BOOST_AUTO_TEST_CASE(op_in_progress_ping)
{
    check_no_modification(detail::ping_algo_params{});
}

BOOST_AUTO_TEST_CASE(op_in_progress_reset_connection)
{
    check_no_modification(detail::reset_connection_algo_params{});
}

BOOST_AUTO_TEST_CASE(op_in_progress_close_statement)
{
    check_no_modification(detail::close_statement_algo_params{3u});
}

// Sanity check: the assertions above are sensitive. With no operation in
// progress, setup() does build the pipeline and overwrite the stages.
BOOST_AUTO_TEST_CASE(not_in_progress_builds_pipeline)
{
    detail::connection_state st(16, 1024, false);
    diagnostics diag;
    prepare_busy_state(st);
    st.data().op_in_progress = false;

    const auto before_stages = st.data().shared_pipeline_stages;
    st.setup(diag, detail::ping_algo_params{});
    const auto after_stages = st.data().shared_pipeline_stages;

    BOOST_TEST(after_stages != before_stages);
    decltype(after_stages) expected{
        detail::pipeline_request_stage{detail::pipeline_stage_kind::ping, 1, {}},
        detail::pipeline_request_stage{}};
    BOOST_TEST(after_stages == expected);
}

BOOST_AUTO_TEST_SUITE_END()
