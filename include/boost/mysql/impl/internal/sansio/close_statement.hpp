//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CLOSE_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CLOSE_STATEMENT_HPP

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/pipeline.hpp>

#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

#include <boost/config.hpp>
#include <boost/system/result.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline system::result<run_pipeline_algo_params> setup_close_statement_pipeline(
    connection_state_data& st,
    close_statement_algo_params params
)
{
    // Pipeline a ping with the close statement, to avoid delays on old connections
    // that don't set tcp_nodelay.
    st.write_buffer.clear();
    auto res1 = serialize_top_level(close_stmt_command{params.stmt_id}, st.write_buffer, 0, st.max_buffer_size());
    if (BOOST_UNLIKELY(res1.err.failed())) return res1.err;
    auto res2 = serialize_top_level(ping_command{}, st.write_buffer);
    if (BOOST_UNLIKELY(res2.err.failed())) return res2.err;
    st.shared_pipeline_stages = {
        {
         {pipeline_stage_kind::close_statement, res1.seqnum, {}},
         {pipeline_stage_kind::ping, res2.seqnum, {}},
         }
    };
    return run_pipeline_algo_params{st.write_buffer, st.shared_pipeline_stages, nullptr};
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
