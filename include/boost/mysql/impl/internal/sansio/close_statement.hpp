//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CLOSE_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CLOSE_STATEMENT_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>
#include <boost/mysql/detail/pipeline.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/run_pipeline.hpp>

#include <array>

namespace boost {
namespace mysql {
namespace detail {

class close_statement_algo
{
    int resume_point_{0};
    std::uint32_t stmt_id_;
    std::array<pipeline_request_step, 2> steps_{};
    run_pipeline_algo pipe_st_{{}};

public:
    close_statement_algo(close_statement_algo_params params) noexcept
        : stmt_id_(params.stmt_id), pipe_st_({params.diag, {}, {}, {}})
    {
    }

    next_action resume(connection_state_data& st, error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            // Setup pipeline.  We pipeline a ping with the close statement
            // to force the server send a response. Otherwise, the client ends up waiting
            // for the next TCP ACK, which takes some milliseconds to be sent
            // (see https://github.com/boostorg/mysql/issues/181)
            // TODO: this doesn't feel right
            st.write_buffer.clear();
            steps_[0].kind = pipeline_step_kind::close_statement;
            steps_[0].seqnum = serialize_close_statement(st.write_buffer, stmt_id_);
            steps_[1].kind = pipeline_step_kind::ping;
            steps_[1].seqnum = serialize_ping(st.write_buffer);
            pipe_st_ = run_pipeline_algo({&pipe_st_.diag(), st.write_buffer, steps_, pipeline_response_ref()}
            );

            // Run it
            while (!(act = pipe_st_.resume(st, ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 1, act)
        }

        return act;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
