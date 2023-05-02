//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_SOME_ROWS_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_READ_SOME_ROWS_HPP

#pragma once

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/network_algorithms/helpers.hpp>
#include <boost/mysql/detail/network_algorithms/read_some_rows.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/asio/post.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

inline rows_view get_some_rows(const channel_base& ch, const execution_state_impl& st)
{
    return rows_view_access::construct(
        ch.shared_fields().data(),
        ch.shared_fields().size(),
        st.meta().size()
    );
}

inline error_code process_some_rows(channel_base& channel, execution_state_impl& st, diagnostics& diag)
{
    // Process all read messages until they run out, an error happens
    // or an EOF is received
    channel.shared_fields().clear();
    output_ref ref(channel.shared_fields());
    while (channel.has_read_messages() && st.is_reading_first())
    {
        auto err = process_row_message(channel, st, diag, ref);
        if (err)
            return err;
    }
    return error_code();
}

inline error_code process_some_rows_static(
    channel_base& channel,
    execution_processor& proc,
    output_ref output,
    std::size_t& read_rows,
    diagnostics& diag
)
{
    // Process all read messages until they run out, an error happens
    // or an EOF is received
    assert(output.offset() == 0u);
    while (channel.has_read_messages() && proc.is_reading_rows() && output.offset() < output.max_size())
    {
        auto err = process_row_message(channel, proc, diag, output);
        if (err)
            return err;
        output.inc_offset();
    }
    read_rows = output.offset();
    return error_code();
}

template <class Stream>
struct read_some_rows_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    diagnostics& diag_;
    execution_state_impl& st_;

    read_some_rows_op(channel<Stream>& chan, diagnostics& diag, execution_state_impl& st) noexcept
        : chan_(chan), diag_(diag), st_(st)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {})
    {
        // Error checking
        if (err)
        {
            self.complete(err, rows_view());
            return;
        }

        // Normal path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            diag_.clear();

            // If we are not reading rows, return
            if (!st_.is_reading_rows())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                self.complete(error_code(), rows_view());
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read at least one message
            BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self));

            // Process messages
            err = process_some_rows(chan_, st_, diag_);
            if (err)
            {
                self.complete(err, rows_view());
                BOOST_ASIO_CORO_YIELD break;
            }

            self.complete(error_code(), get_some_rows(chan_, st_));
        }
    }
};

template <class Stream>
struct read_some_rows_static_op : boost::asio::coroutine
{
    channel<Stream>& chan_;
    diagnostics& diag_;
    execution_processor& st_;
    output_ref output_;

    read_some_rows_static_op(
        channel<Stream>& chan,
        diagnostics& diag,
        execution_processor& st,
        output_ref output
    ) noexcept
        : chan_(chan), diag_(diag), st_(st), output_(output)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code err = {})
    {
        // Error checking
        if (err)
        {
            self.complete(err, 0);
            return;
        }

        // Normal path
        std::size_t read_rows = 0;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            diag_.clear();

            // If we are not reading rows, return
            if (!st_.is_reading_rows())
            {
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                self.complete(error_code(), 0);
                BOOST_ASIO_CORO_YIELD break;
            }

            // Read at least one message
            BOOST_ASIO_CORO_YIELD chan_.async_read_some(std::move(self));

            // Process messages
            err = process_some_rows_static(chan_, st_, output_, read_rows, diag_);
            if (err)
            {
                self.complete(err, 0);
                BOOST_ASIO_CORO_YIELD break;
            }

            self.complete(error_code(), read_rows);
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

template <class Stream>
boost::mysql::rows_view boost::mysql::detail::read_some_rows(
    channel<Stream>& channel,
    execution_state_impl& st,
    error_code& err,
    diagnostics& diag
)
{
    // If we are not reading rows, just return
    if (!st.is_reading_rows())
    {
        return rows_view();
    }

    // Read from the stream until there is at least one message
    channel.read_some(err);
    if (err)
        return rows_view();

    // Process read messages
    err = process_some_rows(channel, st, diag);
    if (err)
        return rows_view();

    return get_some_rows(channel, st);
}

template <
    class Stream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, ::boost::mysql::rows_view))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, boost::mysql::rows_view))
boost::mysql::detail::async_read_some_rows(
    channel<Stream>& channel,
    execution_state_impl& st,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, rows_view)>(
        read_some_rows_op<Stream>(channel, diag, st),
        token,
        channel
    );
}

template <class Stream>
std::size_t boost::mysql::detail::read_some_rows(
    channel<Stream>& channel,
    execution_processor& st,
    const output_ref& output,
    error_code& err,
    diagnostics& diag
)
{
    // If we are not reading rows, just return
    if (!st.is_reading_rows())
    {
        return 0;
    }

    // Read from the stream until there is at least one message
    channel.read_some(err);
    if (err)
        return 0;

    // Process read messages
    std::size_t read_rows = 0;
    err = process_some_rows_static(channel, st, output, read_rows, diag);
    if (err)
        return 0;

    return read_rows;
}

template <
    class Stream,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, std::size_t))
boost::mysql::detail::async_read_some_rows(
    channel<Stream>& channel,
    execution_processor& st,
    const output_ref& output,
    diagnostics& diag,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, std::size_t)>(
        read_some_rows_static_op<Stream>(channel, diag, st, output),
        token,
        channel
    );
}

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_ */
