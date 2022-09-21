//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_IMPL_TEST_STREAM_HPP
#define BOOST_MYSQL_TEST_COMMON_IMPL_TEST_STREAM_HPP

#pragma once

#include "../test_stream.hpp"
#include "buffer_concat.hpp"
#include <boost/mysql/error.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/buffer.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>


boost::mysql::test::test_stream::test_stream(
    fail_count fc,
    executor_type ex
) :
    fail_count_(fc),
    executor_(ex)
{
}

boost::mysql::test::test_stream::test_stream(
    std::vector<std::uint8_t> bytes_to_read,
    fail_count fc,
    executor_type ex
) :
    bytes_to_read_(std::move(bytes_to_read)),
    fail_count_(fc),
    executor_(ex)
{
}

boost::mysql::test::test_stream::test_stream(
    read_behavior b,
    fail_count fc,
    executor_type ex
) :
    bytes_to_read_(std::move(b.bytes_to_read)),
    read_break_offsets_(std::move(b.break_offsets)),
    fail_count_(fc),
    executor_(ex)
{
}

void boost::mysql::test::test_stream::add_message(
    const std::vector<std::uint8_t>& bytes,
    bool separate_reads
)
{
    if (separate_reads)
    {
        read_break_offsets_.insert(bytes_to_read_.size());
    }
    concat(bytes_to_read_, bytes);
}

void boost::mysql::test::test_stream::set_read_behavior(
    read_behavior b
)
{
    bytes_to_read_ = std::move(b.bytes_to_read);
    read_break_offsets_ = std::move(b.break_offsets);
}


template <class MutableBufferSequence>
struct boost::mysql::test::test_stream::read_op : boost::asio::coroutine
{
    test_stream& stream_;
    MutableBufferSequence buffers_;

    read_op(test_stream& stream, const MutableBufferSequence& buffers) : stream_(stream), buffers_(buffers) {};

    template<class Self>
    void operator()(
        Self& self
    )
    {
        error_code err;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));

            err = stream_.fail_count_.maybe_fail();
            if (err)
            {
                self.complete(err, 0);
            }
            else
            {
                self.complete(error_code(), stream_.do_read(buffers_));
            }
        }
    }
};

template <class ConstBufferSequence>
struct boost::mysql::test::test_stream::write_op : boost::asio::coroutine
{
    ConstBufferSequence buffers_;
    test_stream& stream_;

    write_op(test_stream& stream, const ConstBufferSequence& buffers) : stream_(stream), buffers_(buffers) {};

    template<class Self>
    void operator()(
        Self& self
    )
    {
        error_code err;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));

            err = stream_.fail_count_.maybe_fail();
            if (err)
            {
                self.complete(err, 0);
            }
            else
            {
                self.complete(error_code(), stream_.do_write(buffers_));
            }
        }
    }
};

template<
    class MutableBufferSequence,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::mysql::error_code, std::size_t)) CompletionToken
>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, std::size_t))
boost::mysql::test::test_stream::async_read_some(
    const MutableBufferSequence& buffers,
    CompletionToken&& token
)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, std::size_t)>(
        read_op<MutableBufferSequence>(*this, buffers),
        token,
        get_executor()
    );
}

template <class MutableBufferSequence>
std::size_t boost::mysql::test::test_stream::read_some(
    const MutableBufferSequence& buffers,
    error_code& ec
)
{
    error_code err = fail_count_.maybe_fail();
    if (err)
    {
        ec = err;
        return 0;
    }
    else
    {
        ec = error_code();
        return do_read(buffers);
    }
}

template <class ConstBufferSequence>
std::size_t boost::mysql::test::test_stream::write_some(
    const ConstBufferSequence& buffers,
    error_code& ec
)
{
    error_code err = fail_count_.maybe_fail();
    if (err)
    {
        ec = err;
        return 0;
    }
    else
    {
        ec = error_code();
        return do_write(buffers);
    }
}

std::size_t boost::mysql::test::test_stream::get_size_to_read(
    std::size_t buffer_size
) const
{
    auto it = read_break_offsets_.upper_bound(num_bytes_read_);
    std::size_t max_bytes_by_break = it == read_break_offsets_.end() ? std::size_t(-1) : *it - num_bytes_read_;
    return std::min({
        num_unread_bytes(),
        buffer_size,
        max_bytes_by_break
    });
}

template <class MutableBufferSequence>
std::size_t boost::mysql::test::test_stream::do_read(
    const MutableBufferSequence& buffers
)
{
    std::size_t bytes_read = 0;
    auto first = boost::asio::buffer_sequence_begin(buffers);
    auto last = boost::asio::buffer_sequence_end(buffers);

    for (auto it = first; it != last && num_unread_bytes() > 0; ++it)
    {
        boost::asio::mutable_buffer buff = *it;
        std::size_t bytes_to_transfer = get_size_to_read(buff.size());
        std::memcpy(buff.data(), bytes_to_read_.data() + num_bytes_read_, bytes_to_transfer);
        bytes_read += bytes_to_transfer;
        num_bytes_read_ += bytes_to_transfer;
    }

    return bytes_read;
}

template <class ConstBufferSequence>
std::size_t boost::mysql::test::test_stream::do_write(
    const ConstBufferSequence& buffers
)
{
    std::size_t num_bytes_written = 0;
    auto first = boost::asio::buffer_sequence_begin(buffers);
    auto last = boost::asio::buffer_sequence_end(buffers);

    for (auto it = first; it != last && num_bytes_written < write_break_size_; ++it)
    {
        boost::asio::const_buffer buff = *it;
        std::size_t num_bytes_to_transfer = std::min(buff.size(), write_break_size_- num_bytes_written);
        concat(bytes_written_, buff.data(), num_bytes_to_transfer);
        num_bytes_written += num_bytes_to_transfer;
    }

    return num_bytes_written;
}


#endif
