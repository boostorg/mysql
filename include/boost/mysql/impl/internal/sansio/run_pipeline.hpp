//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_RUN_PIPELINE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_RUN_PIPELINE_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/is_fatal_error.hpp>
#include <boost/mysql/pipeline.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/read_execute_response.hpp>
#include <boost/mysql/impl/internal/sansio/read_ok_response.hpp>
#include <boost/mysql/impl/internal/sansio/read_prepare_statement_response.hpp>

#include <boost/variant2/variant.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

struct no_response_algo
{
    next_action resume(error_code ec) { return ec; }
};

struct pipeline_read_resumer
{
    error_code ec;

    template <class Algo>
    next_action operator()(Algo& algo) const
    {
        return algo.resume(ec);
    }
};

class run_pipeline_algo
{
    using any_read_algo = variant2::variant<
        no_response_algo,
        read_execute_response_algo,
        read_prepare_statement_response_algo,
        read_ok_response_algo>;

    connection_state_data* st_;
    int resume_point_{0};
    diagnostics* diag_;
    pipeline* pipe_;
    std::size_t current_step_index_{0};
    error_code ec_;
    any_read_algo read_response_;

    void fail_all_steps(std::size_t first_index, error_code ec)
    {
        for (std::size_t i = 0u; i < first_index; ++i)
        {
            access::get_impl(access::get_impl(*pipe_).steps_[i]).ec = ec;
        }
    }

    void emplace_read_algo()
    {
        auto& step = access::get_impl(access::get_impl(*pipe_).steps_[current_step_index_]);
        switch (step.kind)
        {
        case pipeline_step_kind::execute:
            read_response_.emplace<read_execute_response_algo>(*st_, &step.diag, &step.get_processor());
            break;
        case pipeline_step_kind::prepare_statement:
            read_response_.emplace<read_prepare_statement_response_algo>(*st_, &step.diag)
                .sequence_number() = step.seqnum;
            break;
        case pipeline_step_kind::close_statement:
            // Close statement doesn't have a response
            // TODO: how can we make sure pipelines never end with a close_statement? This can cause massive
            // delays
            read_response_.emplace<no_response_algo>();
            break;
        case pipeline_step_kind::set_character_set:
        case pipeline_step_kind::reset_connection:
        case pipeline_step_kind::ping: read_response_.emplace<read_ok_response_algo>(*st_, &step.diag); break;
        default: BOOST_ASSERT(false);
        }
    }

    void propagate_step_results()
    {
        auto& step = access::get_impl(access::get_impl(*pipe_).steps_[current_step_index_]);
        switch (step.kind)
        {
        case pipeline_step_kind::prepare_statement:
            step.result = variant2::get<read_prepare_statement_response_algo>(read_response_).result();
            break;
        case pipeline_step_kind::set_character_set:
            // TODO: having this duplicated here and in set_character_set is error prone
            conn_state().current_charset = variant2::get<character_set>(step.result);
            break;
        default: break;
        }
    }

    next_action resume_read_algo(error_code ec)
    {
        return variant2::visit(pipeline_read_resumer{ec}, read_response_);
    }

public:
    run_pipeline_algo(connection_state_data& st, run_pipeline_algo_params params) noexcept
        : st_(&st), diag_(params.diag), pipe_(params.pipe)
    {
    }

    connection_state_data& conn_state() { return *st_; }

    next_action resume(error_code ec)
    {
        auto& pipe_impl = access::get_impl(*pipe_);
        next_action act;

        switch (resume_point_)
        {
        case 0:

            // Clear previous state
            diag_->clear();
            pipe_impl.reset_results();

            // Write the request
            st_->writer.prepare_pipelined_write(pipe_impl.buffer_);
            BOOST_MYSQL_YIELD(resume_point_, 1, next_action::write({}))

            // If writing the request failed, fail all the steps and return
            if (ec)
            {
                fail_all_steps(0, ec);
                return ec;
            }

            // For each step
            for (; current_step_index_ < access::get_impl(*pipe_).steps_.size(); ++current_step_index_)
            {
                // Create a suitable algo
                emplace_read_algo();

                // Run it until completion
                ec.clear();
                while (!(act = resume_read_algo(ec)).is_done())
                    BOOST_MYSQL_YIELD(resume_point_, 2, act)

                // TODO: ec_ is a little bit bug-prone
                if (act.error())
                {
                    // The first error we encounter is the result of the entire operation
                    if (!ec_)
                    {
                        ec_ = act.error();
                        *diag_ = access::get_impl(*pipe_).steps_[current_step_index_].diag();
                    }

                    // If the error was fatal, don't attempt further steps
                    if (is_fatal_error(act.error()))
                    {
                        fail_all_steps(current_step_index_, act.error());
                        return act.error();
                    }
                }
                else
                {
                    propagate_step_results();
                }
            }

            return ec_;
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
