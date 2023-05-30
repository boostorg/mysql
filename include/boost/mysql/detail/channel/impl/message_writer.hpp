//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_IMPL_MESSAGE_WRITER_HPP
#define BOOST_MYSQL_DETAIL_CHANNEL_IMPL_MESSAGE_WRITER_HPP

#pragma once

#include <boost/mysql/detail/channel/any_stream.hpp>
#include <boost/mysql/detail/channel/message_writer.hpp>

#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>

void boost::mysql::detail::message_writer::write(any_stream& stream, error_code& ec)
{
    while (!processor_.done())
    {
        auto msg = processor_.next_chunk();
        std::size_t bytes_written = stream.write_some(msg, ec);
        if (ec)
            break;
        processor_.on_bytes_written(bytes_written);
    }
}

struct boost::mysql::detail::message_writer::write_op : boost::asio::coroutine
{
    any_stream& stream_;
    message_writer_processor& processor_;

    write_op(any_stream& stream, message_writer_processor& processor) noexcept
        : stream_(stream), processor_(processor)
    {
    }

    template <class Self>
    void operator()(Self& self, error_code ec = {}, std::size_t bytes_written = 0)
    {
        // Error handling
        if (ec)
        {
            self.complete(ec);
            return;
        }

        // Non-error path
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // done() never returns false after a call to prepare_buffer(), so no post() needed
            BOOST_ASSERT(!processor_.done());
            while (!processor_.done())
            {
                BOOST_ASIO_CORO_YIELD stream_.async_write_some(processor_.next_chunk(), std::move(self));
                processor_.on_bytes_written(bytes_written);
            };

            self.complete(error_code());
        }
    }
};

template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
boost::mysql::detail::message_writer::async_write(any_stream& stream, CompletionToken&& token)
{
    return boost::asio::async_compose<CompletionToken, void(error_code)>(
        write_op(stream, processor_),
        token,
        stream
    );
}

#endif
