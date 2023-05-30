//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_START_EXECUTION_IMPL_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_START_EXECUTION_IMPL_HPP

#pragma once

#include <boost/mysql/client_errc.hpp>

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/network_algorithms/read_resultset_head.hpp>
#include <boost/mysql/detail/network_algorithms/start_execution_impl.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/detail/protocol/query_messages.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline error_code check_client_errors(const any_execution_request& req)
{
    if (req.is_query)
        return error_code();
    return req.data.stmt.stmt.num_params() == req.data.stmt.params.size() ? error_code()
                                                                          : client_errc::wrong_num_params;
}

inline resultset_encoding get_encoding(const any_execution_request& req)
{
    return req.is_query ? resultset_encoding::text : resultset_encoding::binary;
}

inline void serialize_execution_request(
    const any_execution_request& req,
    channel& chan,
    std::uint8_t& sequence_number
)
{
    if (req.is_query)
    {
        chan.serialize(com_query_packet{string_eof(req.data.query)}, sequence_number);
    }
    else
    {
        com_stmt_execute_packet<const field_view*> request{
            req.data.stmt.stmt.id(),
            std::uint8_t(0),   // flags
            std::uint32_t(1),  // iteration count
            std::uint8_t(1),   // new params flag: set
            req.data.stmt.params.begin(),
            req.data.stmt.params.end(),
        };
        chan.serialize(request, sequence_number);
    }
}

inline void execution_setup(const any_execution_request& req, channel& chan, execution_processor& proc)
{
    // Reeset the processor
    proc.reset(get_encoding(req), chan.meta_mode());

    // Serialize the execution request
    serialize_execution_request(req, chan, proc.sequence_number());
}

struct start_execution_impl_op : boost::asio::coroutine
{
    channel& chan_;
    any_execution_request req_;
    execution_processor& proc_;
    diagnostics& diag_;

    start_execution_impl_op(
        channel& chan,
        const any_execution_request& req,
        execution_processor& proc,
        diagnostics& diag
    )
        : chan_(chan), req_(req), proc_(proc), diag_(diag)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {})
    {
        // Error checking
        if (err)
        {
            self.complete(err);
            return;
        }

        // Non-error path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            diag_.clear();

            // Check for errors
            err = check_client_errors(req_);
            if (err)
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(chan_.get_executor(), std::move(self));
                self.complete(err);
                BOOST_ASIO_CORO_YIELD break;
            }

            // Setup
            execution_setup(req_, chan_, proc_);

            // Send the execution request (serialized by setup)
            BOOST_ASIO_CORO_YIELD chan_.async_write(std::move(self));

            // Read the first resultset's head
            BOOST_ASIO_CORO_YIELD
            async_read_resultset_head_impl(chan_, proc_, diag_, std::move(self));

            self.complete(error_code());
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

void boost::mysql::detail::start_execution_impl(
    channel& channel,
    const any_execution_request& req,
    execution_processor& proc,
    error_code& err,
    diagnostics& diag
)
{
    diag.clear();

    // Check for errors
    err = check_client_errors(req);
    if (err)
        return;

    // Setup
    execution_setup(req, channel, proc);

    // Send the execution request (serialized by setup)
    channel.write(err);
    if (err)
        return;

    // Read the first resultset's head
    read_resultset_head_impl(channel, proc, err, diag);
    if (err)
        return;
}

void boost::mysql::detail::async_start_execution_impl(
    channel& channel,
    const any_execution_request& req,
    execution_processor& proc,
    diagnostics& diag,
    asio::any_completion_handler<void(error_code)> handler
)
{
    return boost::asio::async_compose<asio::any_completion_handler<void(error_code)>, void(error_code)>(
        start_execution_impl_op(channel, req, proc, diag),
        handler,
        channel
    );
}

#endif
