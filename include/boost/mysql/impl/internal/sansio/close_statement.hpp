//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
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
#include <boost/mysql/impl/internal/sansio/create_stage.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline run_pipeline_algo_params setup_close_statement_pipeline(
    connection_state_data& st,
    close_statement_algo_params params
)
{
    st.write_buffer.clear();
    auto stage1 = create_stage(
        pipeline_stage_kind::close_statement,
        serialize_top_level(close_stmt_command{params.stmt_id}, st.write_buffer),
        {}
    );
    auto stage2 = create_stage(
        pipeline_stage_kind::ping,
        serialize_top_level(ping_command{}, st.write_buffer),
        {}
    );
    st.shared_pipeline_stages = {stage1, stage2};
    return {st.write_buffer, st.shared_pipeline_stages, nullptr};
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
