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
#include <boost/mysql/impl/internal/sansio/ping.hpp>
#include <boost/mysql/impl/internal/sansio/prepare_statement.hpp>
#include <boost/mysql/impl/internal/sansio/read_execute_response.hpp>
#include <boost/mysql/impl/internal/sansio/reset_connection.hpp>
#include <boost/mysql/impl/internal/sansio/set_character_set.hpp>

#include <boost/core/span.hpp>
#include <boost/variant2/variant.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

// TODO: running a pipeline without stages is possible - do we need any special handling?
class run_pipeline_algo
{
    struct no_response_algo
    {
        next_action resume(connection_state_data&, error_code ec) { return ec; }
    };

    using any_read_algo = variant2::variant<
        no_response_algo,                      // close statement
        read_execute_response_algo,            // execute
        read_prepare_statement_response_algo,  // prepare statement
        read_reset_connection_response_algo,   // reset connection
        read_ping_response_algo,               // ping
        read_set_character_set_response_algo   // set character set
        >;

    diagnostics* diag_;
    diagnostics temp_diag_;
    span<const std::uint8_t> request_buffer_;
    span<const detail::pipeline_request_stage> stages_;
    detail::pipeline_response_ref response_;

    int resume_point_{0};
    std::size_t current_stage_index_{0};
    error_code pipeline_ec_;  // Result of the entire operation
    bool has_hatal_error_{};  // If true, fail further stages with client_errc::cancelled
    any_read_algo read_response_algo_;

    void setup_current_stage(const connection_state_data& st)
    {
        // Reset previous data
        temp_diag_.clear();

        // Setup read algo
        auto stage = stages_[current_stage_index_];
        switch (stage.kind)
        {
        case pipeline_stage_kind::execute:
        {
            auto& processor = response_.get_processor(current_stage_index_);
            processor.reset(stage.stage_specific.enc, st.meta_mode);
            processor.sequence_number() = stage.seqnum;
            read_response_algo_.emplace<read_execute_response_algo>(&temp_diag_, &processor);
            break;
        }
        case pipeline_stage_kind::prepare_statement:
            read_response_algo_.emplace<read_prepare_statement_response_algo>(&temp_diag_, stage.seqnum);
            break;
        case pipeline_stage_kind::close_statement:
            // Close statement doesn't have a response
            // TODO: this causes delays unless we disable naggle's algorithm
            read_response_algo_.emplace<no_response_algo>();
            break;
        case pipeline_stage_kind::set_character_set:
            read_response_algo_.emplace<read_set_character_set_response_algo>(
                &temp_diag_,
                stage.stage_specific.charset,
                stage.seqnum
            );
            break;
        case pipeline_stage_kind::reset_connection:
            read_response_algo_.emplace<read_reset_connection_response_algo>(&temp_diag_, stage.seqnum);
            break;
        case pipeline_stage_kind::ping:
            read_response_algo_.emplace<read_ping_response_algo>(&temp_diag_, stage.seqnum);
            break;
        default: BOOST_ASSERT(false);
        }
    }

    void on_stage_finished(const connection_state_data& st, error_code stage_ec)
    {
        if (stage_ec)
        {
            // If the error was fatal, fail successive stages with a cancelled error.
            // This error is the result of the operation
            if (is_fatal_error(stage_ec))
            {
                pipeline_ec_ = stage_ec;
                *diag_ = temp_diag_;
                has_hatal_error_ = true;
            }
            else if (!pipeline_ec_)
            {
                // In the absence of fatal errors, the first error we encounter is the result of the operation
                pipeline_ec_ = stage_ec;
                *diag_ = temp_diag_;
            }

            // Propagate the error
            response_.set_error(current_stage_index_, stage_ec, std::move(temp_diag_));
        }
        else
        {
            if (stages_[current_stage_index_].kind == pipeline_stage_kind::prepare_statement)
            {
                // Propagate results. We don't support prepare statements
                // with responses having has_value() == false
                response_.set_result(
                    current_stage_index_,
                    variant2::get<read_prepare_statement_response_algo>(read_response_algo_).result(st)
                );
            }
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
        : diag_(params.diag),
          request_buffer_(params.request_buffer),
          stages_(params.request_stages),
          response_(params.response)
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
            response_.setup(stages_);

            // Write the request
            st.writer.reset(request_buffer_);
            BOOST_MYSQL_YIELD(resume_point_, 1, next_action::write({}))

            // If writing the request failed, fail all the stages with the given error code
            if (ec)
            {
                pipeline_ec_ = ec;
                has_hatal_error_ = true;
            }

            // For each stage
            for (; current_stage_index_ < stages_.size(); ++current_stage_index_)
            {
                // If there was a fatal error, just set the error and move forward
                if (has_hatal_error_)
                {
                    response_.set_error(current_stage_index_, client_errc::cancelled, {});
                    continue;
                }

                // Setup the stage
                setup_current_stage(st);

                // Run it until completion
                ec.clear();
                while (!(act = resume_read_algo(st, ec)).is_done())
                    BOOST_MYSQL_YIELD(resume_point_, 2, act)

                // Process the stage's result
                on_stage_finished(st, act.error());
            }
        }

        return pipeline_ec_;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
