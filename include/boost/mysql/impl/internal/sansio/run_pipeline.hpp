//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_RUN_PIPELINE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_RUN_PIPELINE_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/is_fatal_error.hpp>
#include <boost/mysql/pipeline.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>
#include <boost/mysql/detail/pipeline.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/read_execute_response.hpp>
#include <boost/mysql/impl/internal/sansio/read_ok_response.hpp>
#include <boost/mysql/impl/internal/sansio/read_prepare_statement_response.hpp>

#include <boost/variant2/variant.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

class run_pipeline_algo
{
    struct no_response_algo
    {
        next_action resume(connection_state_data&, error_code ec) { return ec; }
    };

    using any_read_algo = variant2::variant<
        no_response_algo,
        read_execute_response_algo,
        read_prepare_statement_response_algo,
        read_ok_response_algo>;

    diagnostics* diag_;
    pipeline_step_generator step_gen_;
    const std::vector<std::uint8_t>* buffer_;

    int resume_point_{0};
    pipeline_step_descriptor current_step_{};
    std::size_t current_step_index_{0};
    error_code pipeline_ec_;   // Result of the entire operation
    bool fatal_error_{false};  // If true, don't process further steps
    any_read_algo read_response_algo_;

    void setup_current_step()
    {
        // Reset previous data
        current_step_.err->ec.clear();
        current_step_.err->diag.clear();

        // Setup read algo
        switch (current_step_.kind)
        {
        case pipeline_step_kind::execute:
            current_step_.step_specific.execute.processor->reset(
                current_step_.step_specific.execute.encoding,
                current_step_.step_specific.execute.meta
            );
            current_step_.step_specific.execute.processor->sequence_number() = current_step_.seqnum;
            read_response_algo_.emplace<read_execute_response_algo>(
                &current_step_.err->diag,
                current_step_.step_specific.execute.processor
            );
            break;
        case pipeline_step_kind::prepare_statement:
            read_response_algo_.emplace<read_prepare_statement_response_algo>(&current_step_.err->diag)
                .sequence_number() = current_step_.seqnum;
            break;
        case pipeline_step_kind::close_statement:
            // Close statement doesn't have a response
            // TODO: how can we make sure pipelines never end with a close_statement? This can cause massive
            // delays
            read_response_algo_.emplace<no_response_algo>();
            break;
        case pipeline_step_kind::set_character_set:
        case pipeline_step_kind::reset_connection:
        case pipeline_step_kind::ping:
            read_response_algo_.emplace<read_ok_response_algo>(&current_step_.err->diag)
                .sequence_number() = current_step_.seqnum;
            break;
        default: BOOST_ASSERT(false);
        }
    }

    void propagate_step_results(connection_state_data& st)
    {
        switch (current_step_.kind)
        {
        case pipeline_step_kind::prepare_statement:
            *current_step_.step_specific.prepare_statement
                 .stmt = variant2::get<read_prepare_statement_response_algo>(read_response_algo_).result(st);
            break;
        case pipeline_step_kind::set_character_set:
            // TODO: having this duplicated here and in set_character_set is error prone
            st.current_charset = current_step_.step_specific.set_character_set.charset;
            break;
        default: break;
        }
    }

    void on_step_finished(connection_state_data& st, error_code step_ec)
    {
        if (step_ec)
        {
            // The first error we encounter is the result of the entire operation
            if (!pipeline_ec_)
            {
                pipeline_ec_ = step_ec;
                *diag_ = current_step_.err->diag;
            }

            // If the error was fatal, record it so we don't attempt further steps
            if (is_fatal_error(step_ec))
            {
                fatal_error_ = true;
            }
        }
        else
        {
            propagate_step_results(st);
        }
    }

    struct resume_visitor
    {
        connection_state_data& st;
        error_code ec;

        template <class Algo>
        next_action operator()(Algo& algo) const
        {
            return algo.resume(st, ec);
        }
    };

    next_action resume_read_algo(connection_state_data& st, error_code ec)
    {
        return variant2::visit(resume_visitor{st, ec}, read_response_algo_);
    }

public:
    run_pipeline_algo(run_pipeline_algo_params params) noexcept
        : diag_(params.diag), step_gen_(params.step_gen), buffer_(params.buffer)
    {
    }

    next_action resume(connection_state_data& st, error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            // Clear previous state
            diag_->clear();

            // Write the request
            st.writer.prepare_pipelined_write(*buffer_);
            BOOST_MYSQL_YIELD(resume_point_, 1, next_action::write({}))

            // If writing the request failed, fail all the steps and return
            if (ec)
            {
                pipeline_ec_ = ec;
                fatal_error_ = true;
            }

            // For each step
            while (true)
            {
                // Get the next step
                current_step_ = step_gen_.step_descriptor_at(current_step_index_++);
                if (!current_step_.valid())
                    break;

                // If there was a fatal error, just set the error and move forward
                if (fatal_error_)
                {
                    current_step_.err->ec = client_errc::cancelled;  // TODO: we need a new error code here?
                    current_step_.err->diag.clear();
                    continue;
                }

                // Setup the step
                setup_current_step();

                // Run it until completion
                ec.clear();
                while (!(act = resume_read_algo(st, ec)).is_done())
                    BOOST_MYSQL_YIELD(resume_point_, 2, act)

                // Process the step's result
                on_step_finished(st, act.error());
            }
        }

        return pipeline_ec_;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
