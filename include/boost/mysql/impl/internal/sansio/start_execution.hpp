//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_START_EXECUTION_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_START_EXECUTION_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/next_action.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/query_with_params.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/message_reader.hpp>
#include <boost/mysql/impl/internal/sansio/read_resultset_head.hpp>

namespace boost {
namespace mysql {
namespace detail {

class start_execution_algo
{
    int resume_point_{0};
    read_resultset_head_algo read_head_st_;
    any_execution_request req_;

    std::uint8_t& seqnum() { return processor().sequence_number(); }
    execution_processor& processor() { return read_head_st_.processor(); }
    diagnostics& diag() { return read_head_st_.diag(); }

    static resultset_encoding get_encoding(any_execution_request::type_t type)
    {
        switch (type)
        {
        case any_execution_request::type_t::query:
        case any_execution_request::type_t::query_with_params: return resultset_encoding::text;
        case any_execution_request::type_t::stmt: return resultset_encoding::binary;
        default: BOOST_ASSERT(false); return resultset_encoding::text;
        }
    }

    next_action write_query_with_params(
        connection_state_data& st,
        any_execution_request::data_t::query_with_params_t data
    )
    {
        // TODO: this should be expressible in terms of st.write()
        // Determine format options
        if (st.current_charset.name == nullptr)
        {
            return error_code(client_errc::unknown_character_set);
        }

        // Clear the write buffer
        st.write_buffer.clear();

        // Serialize
        auto res = serialize_query_with_params(
            st.write_buffer,
            data.query,
            data.args,
            format_options{st.current_charset, st.backslash_escapes}
        );

        // Check for errors
        if (res.has_error())
            return res.error();

        // Done
        seqnum() = *res;
        return next_action::write({st.write_buffer, false});
    }

    next_action write_stmt(connection_state_data& st, any_execution_request::data_t::stmt_t data)
    {
        if (data.stmt.num_params() != data.params.size())
            return error_code(client_errc::wrong_num_params);
        return st.write(execute_stmt_command{data.stmt.id(), data.params}, seqnum());
    }

    next_action compose_request(connection_state_data& st)
    {
        switch (req_.type)
        {
        case any_execution_request::type_t::query: return st.write(query_command{req_.data.query}, seqnum());
        case any_execution_request::type_t::query_with_params:
            return write_query_with_params(st, req_.data.query_with_params);
        case any_execution_request::type_t::stmt: return write_stmt(st, req_.data.stmt);
        default: BOOST_ASSERT(false); return next_action();
        }
    }

public:
    start_execution_algo(diagnostics& diag, start_execution_algo_params params) noexcept
        : read_head_st_(diag, {params.proc}), req_(params.req)
    {
    }

    next_action resume(connection_state_data& st, error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            // Clear diagnostics
            diag().clear();

            // Reset the processor
            processor().reset(get_encoding(req_.type), st.meta_mode);

            // Send the execution request
            BOOST_MYSQL_YIELD(resume_point_, 1, compose_request(st))
            if (ec)
                return ec;

            // Read the first resultset's head and return its result
            while (!(act = read_head_st_.resume(st, ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 2, act)
            return act;
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_RESULTSET_HEAD_HPP_ */
