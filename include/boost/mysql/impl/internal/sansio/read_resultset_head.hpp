//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_RESULTSET_HEAD_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_RESULTSET_HEAD_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline error_code process_execution_response(
    connection_state_data& st,
    execution_processor& proc,
    span<const std::uint8_t> msg,
    diagnostics& diag
)
{
    auto response = deserialize_execute_response(msg, st.flavor, diag);
    error_code err;
    switch (response.type)
    {
    case execute_response::type_t::error: err = response.data.err; break;
    case execute_response::type_t::ok_packet:
        st.backslash_escapes = response.data.ok_pack.backslash_escapes();
        err = proc.on_head_ok_packet(response.data.ok_pack, diag);
        break;
    case execute_response::type_t::num_fields: proc.on_num_meta(response.data.num_fields); break;
    }
    return err;
}

inline error_code process_field_definition(
    execution_processor& proc,
    span<const std::uint8_t> msg,
    diagnostics& diag
)
{
    // Deserialize the message
    coldef_view coldef{};
    auto err = deserialize_column_definition(msg, coldef);
    if (err)
        return err;

    // Notify the processor
    return proc.on_meta(coldef, diag);
}

class read_resultset_head_algo
{
    connection_state_data* st_;
    int resume_point_{0};
    diagnostics* diag_;
    execution_processor* proc_;

public:
    read_resultset_head_algo(connection_state_data& st, read_resultset_head_algo_params params) noexcept
        : st_(&st), diag_(params.diag), proc_(params.proc)
    {
    }

    void reset() { resume_point_ = 0; }

    connection_state_data& conn_state() { return *st_; }
    diagnostics& diag() { return *diag_; }
    execution_processor& processor() { return *proc_; }

    next_action resume(error_code ec)
    {
        if (ec)
            return ec;

        switch (resume_point_)
        {
        case 0:

            // Clear diagnostics
            diag_->clear();

            // If we're not reading head, return
            if (!proc_->is_reading_head())
                return next_action();

            // Read the response
            BOOST_MYSQL_YIELD(resume_point_, 1, st_->read(proc_->sequence_number()))

            // Response may be: ok_packet, err_packet, local infile request
            // (not implemented), or response with fields
            ec = process_execution_response(*st_, *proc_, st_->reader.message(), *diag_);
            if (ec)
                return ec;

            // Read all of the field definitions
            while (proc_->is_reading_meta())
            {
                // Read a message
                BOOST_MYSQL_YIELD(resume_point_, 2, st_->read(proc_->sequence_number()))

                // Process the metadata packet
                ec = process_field_definition(*proc_, st_->reader.message(), *diag_);
                if (ec)
                    return ec;
            }

            // No EOF packet is expected here, as we require deprecate EOF capabilities
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
