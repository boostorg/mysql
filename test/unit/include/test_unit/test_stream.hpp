//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_TEST_STREAM_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_TEST_STREAM_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/any_stream.hpp>

#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>

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

class test_stream final : public detail::any_stream
{
public:
    test_stream() = default;

    // Setters
    test_stream& add_message(const std::vector<std::uint8_t>& bytes);
    test_stream& add_message_part(const std::vector<std::uint8_t>& bytes);
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

    // Executor
    executor_type get_executor() override final { return executor_; }

    // SSL
    bool supports_ssl() const noexcept override final;
    void handshake(error_code&) override final;
    void async_handshake(asio::any_completion_handler<void(error_code)>) override final;
    void shutdown(error_code&) override final;
    void async_shutdown(asio::any_completion_handler<void(error_code)>) override final;

    // Reading
    std::size_t read_some(asio::mutable_buffer, error_code& ec) override final;
    void async_read_some(asio::mutable_buffer, asio::any_completion_handler<void(error_code, std::size_t)>)
        override final;

    // Writing
    std::size_t write_some(boost::asio::const_buffer, error_code& ec) override final;
    void async_write_some(boost::asio::const_buffer, asio::any_completion_handler<void(error_code, std::size_t)>)
        override final;

    // Connect and close
    void connect(const void*, error_code&) override final;
    void async_connect(const void*, asio::any_completion_handler<void(error_code)>) override final;
    void close(error_code&) override final;
    bool is_open() const noexcept override final;

private:
    std::vector<std::uint8_t> bytes_to_read_;
    std::set<std::size_t> read_break_offsets_;
    std::size_t num_bytes_read_{0};
    std::vector<std::uint8_t> bytes_written_;
    fail_count fail_count_;
    std::size_t write_break_size_{1024};  // max number of bytes to be written in each write_some
    executor_type executor_;

    std::size_t get_size_to_read(std::size_t buffer_size) const;
    std::size_t do_read(asio::mutable_buffer buff, error_code& ec);
    std::size_t do_write(asio::const_buffer buff, error_code& ec);

    struct read_op;
    struct write_op;
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif