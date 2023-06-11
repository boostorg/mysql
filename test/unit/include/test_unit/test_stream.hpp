//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_TEST_STREAM_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_TEST_STREAM_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/system_executor.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <vector>

#include "test_common/buffer_concat.hpp"

namespace boost {
namespace mysql {
namespace test {

// Inspired by Beast's fail count
class fail_count
{
    std::size_t fail_after_;
    std::size_t num_calls_{0};
    error_code err_;

public:
    static constexpr std::size_t never_fail = std::size_t(-1);
    explicit fail_count(
        std::size_t fail_after = never_fail,
        error_code err = make_error_code(std::errc::io_error)
    ) noexcept
        : fail_after_(fail_after), err_(err)
    {
    }
    error_code maybe_fail() noexcept { return ++num_calls_ >= fail_after_ ? err_ : error_code(); }
};

class test_stream
{
public:
    // Executors
    using executor_type = boost::asio::any_io_executor;
    executor_type get_executor() noexcept { return executor_; }

    using lowest_layer_type = test_stream;
    lowest_layer_type& lowest_layer() { return *this; }

    struct read_behavior
    {
        std::vector<std::uint8_t> bytes_to_read;
        std::set<std::size_t> break_offsets;

        read_behavior() = default;
        read_behavior(std::vector<std::uint8_t> bytes, std::set<std::size_t> offsets = {})
            : bytes_to_read(std::move(bytes)), break_offsets(std::move(offsets))
        {
            assert(break_offsets.empty() || *break_offsets.rbegin() < bytes_to_read.size());
        }
    };

    // Constructors
    test_stream(fail_count fc = fail_count(), executor_type ex = boost::asio::system_executor())
        : fail_count_(fc), executor_(ex)
    {
    }

    test_stream(
        std::vector<std::uint8_t> bytes_to_read,
        fail_count fc = fail_count(),
        executor_type ex = boost::asio::system_executor()
    )
        : bytes_to_read_(std::move(bytes_to_read)), fail_count_(fc), executor_(ex)
    {
    }

    test_stream(
        read_behavior b,
        fail_count fc = fail_count(),
        executor_type ex = boost::asio::system_executor()
    )
        : bytes_to_read_(std::move(b.bytes_to_read)),
          read_break_offsets_(std::move(b.break_offsets)),
          fail_count_(fc),
          executor_(ex)
    {
    }

    // Setting test behavior
    inline test_stream& add_message(const std::vector<std::uint8_t>& bytes, bool separate_reads = true);
    test_stream& set_write_break_size(std::size_t size) noexcept
    {
        write_break_size_ = size;
        return *this;
    }
    test_stream& set_fail_count(const fail_count& fc) noexcept
    {
        fail_count_ = fc;
        return *this;
    }

    // Getting test results
    std::size_t num_bytes_read() const noexcept { return num_bytes_read_; }
    std::size_t num_unread_bytes() const noexcept { return bytes_to_read_.size() - num_bytes_read_; }
    const std::vector<std::uint8_t>& bytes_written() const noexcept { return bytes_written_; }

    // Stream operations
    template <class MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers, error_code& ec)
    {
        return do_read(buffers, ec);
    }

    template <class ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers, error_code& ec)
    {
        return do_write(buffers, ec);
    }

    template <
        class MutableBufferSequence,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, std::size_t)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::size_t))
    async_read_some(const MutableBufferSequence& buffers, CompletionToken&& token);

    template <
        class ConstBufferSequence,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, std::size_t)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, std::size_t))
    async_write_some(ConstBufferSequence const& buffers, CompletionToken&& token);

private:
    std::vector<std::uint8_t> bytes_to_read_;
    std::set<std::size_t> read_break_offsets_;
    std::size_t num_bytes_read_{0};
    std::vector<std::uint8_t> bytes_written_;
    fail_count fail_count_;
    std::size_t write_break_size_{1024};  // max number of bytes to be written in each write_some
    executor_type executor_;

    template <class MutableBufferSequence>
    struct read_op;
    template <class ConstBufferSequence>
    struct write_op;

    inline std::size_t get_size_to_read(std::size_t buffer_size) const;

    template <class MutableBufferSequence>
    std::size_t do_read(const MutableBufferSequence& buffers, error_code& ec);

    template <class ConstBufferSequence>
    std::size_t do_write(const ConstBufferSequence& buffers, error_code& ec);
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

// Implementations
boost::mysql::test::test_stream& boost::mysql::test::test_stream::add_message(
    const std::vector<std::uint8_t>& bytes,
    bool separate_reads
)
{
    if (separate_reads)
    {
        read_break_offsets_.insert(bytes_to_read_.size());
    }
    concat(bytes_to_read_, bytes);
    return *this;
}

template <class MutableBufferSequence>
struct boost::mysql::test::test_stream::read_op : boost::asio::coroutine
{
    test_stream& stream_;
    MutableBufferSequence buffers_;

    read_op(test_stream& stream, const MutableBufferSequence& buffers) : stream_(stream), buffers_(buffers){};

    template <class Self>
    void operator()(Self& self)
    {
        error_code err;
        std::size_t bytes_read;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_ASIO_CORO_YIELD boost::asio::post(stream_.get_executor(), std::move(self));

            bytes_read = stream_.do_read(buffers_, err);
            self.complete(err, bytes_read);
        }
    }
};

template <
    class MutableBufferSequence,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, std::size_t)) CompletionToken>
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

template <class ConstBufferSequence>
struct boost::mysql::test::test_stream::write_op : boost::asio::coroutine
{
    test_stream& stream_;
    ConstBufferSequence buffers_;

    write_op(test_stream& stream, const ConstBufferSequence& buffers) : stream_(stream), buffers_(buffers){};

    template <class Self>
    void operator()(Self& self)
    {
        error_code err;
        std::size_t bytes_written;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_ASIO_CORO_YIELD boost::asio::post(stream_.get_executor(), std::move(self));

            bytes_written = stream_.do_write(buffers_, err);
            self.complete(err, bytes_written);
        }
    }
};

template <
    class ConstBufferSequence,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code, std::size_t)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, std::size_t))
boost::mysql::test::test_stream::async_write_some(const ConstBufferSequence& buffers, CompletionToken&& token)
{
    return boost::asio::async_compose<CompletionToken, void(error_code, std::size_t)>(
        write_op<ConstBufferSequence>(*this, buffers),
        token,
        get_executor()
    );
}

std::size_t boost::mysql::test::test_stream::get_size_to_read(std::size_t buffer_size) const
{
    auto it = read_break_offsets_.upper_bound(num_bytes_read_);
    std::size_t max_bytes_by_break = it == read_break_offsets_.end() ? std::size_t(-1)
                                                                     : *it - num_bytes_read_;
    return (std::min)({num_unread_bytes(), buffer_size, max_bytes_by_break});
}

template <class MutableBufferSequence>
std::size_t boost::mysql::test::test_stream::do_read(const MutableBufferSequence& buffers, error_code& ec)
{
    // Fail count
    error_code err = fail_count_.maybe_fail();
    if (err)
    {
        ec = err;
        return 0;
    }

    // If the user requested some bytes but we don't have any,
    // fail. In the real world, the stream would block until more
    // bytes are received, but this is a test, and this condition
    // indicates an error.
    auto first = boost::asio::buffer_sequence_begin(buffers);
    auto last = boost::asio::buffer_sequence_end(buffers);
    if (num_unread_bytes() == 0)
    {
        for (auto it = first; it != last; ++it)
        {
            if (it->size() != 0)
            {
                ec = boost::asio::error::eof;
                return 0;
            }
        }
    }

    // Actually read
    std::size_t bytes_read = 0;
    for (auto it = first; it != last && num_unread_bytes() > 0; ++it)
    {
        boost::asio::mutable_buffer buff = *it;
        std::size_t bytes_to_transfer = get_size_to_read(buff.size());
        std::memcpy(buff.data(), bytes_to_read_.data() + num_bytes_read_, bytes_to_transfer);
        bytes_read += bytes_to_transfer;
        num_bytes_read_ += bytes_to_transfer;
    }
    ec = error_code();

    return bytes_read;
}

template <class ConstBufferSequence>
std::size_t boost::mysql::test::test_stream::do_write(const ConstBufferSequence& buffers, error_code& ec)
{
    // Fail count
    error_code err = fail_count_.maybe_fail();
    if (err)
    {
        ec = err;
        return 0;
    }

    // Actually write
    std::size_t num_bytes_written = 0;
    auto first = boost::asio::buffer_sequence_begin(buffers);
    auto last = boost::asio::buffer_sequence_end(buffers);
    for (auto it = first; it != last && num_bytes_written < write_break_size_; ++it)
    {
        boost::asio::const_buffer buff = *it;
        std::size_t num_bytes_to_transfer = (std::min)(buff.size(), write_break_size_ - num_bytes_written);
        concat(bytes_written_, buff.data(), num_bytes_to_transfer);
        num_bytes_written += num_bytes_to_transfer;
    }

    ec = error_code();

    return num_bytes_written;
}

#endif