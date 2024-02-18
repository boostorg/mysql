//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/sansio/algo_runner.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/message_reader.hpp>
#include <boost/mysql/impl/internal/sansio/next_action.hpp>
#include <boost/mysql/impl/internal/sansio/sansio_algorithm.hpp>

#include <boost/asio/coroutine.hpp>
#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "test_common/assert_buffer_equals.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/mock_message.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using boost::span;
using boost::asio::coroutine;
using boost::mysql::client_errc;
using boost::mysql::error_code;
using u8vec = std::vector<std::uint8_t>;

BOOST_AUTO_TEST_SUITE(test_algo_runner)

void transfer(span<std::uint8_t> buff, span<const std::uint8_t> bytes)
{
    assert(buff.size() >= bytes.size());
    std::memcpy(buff.data(), bytes.data(), bytes.size());
}

void transfer(span<std::uint8_t> buff, const std::vector<std::uint8_t>& bytes)
{
    transfer(buff, boost::span<const std::uint8_t>(bytes));
}

const u8vec msg1{0x01, 0x02, 0x03};
const u8vec msg2(50, 0x04);

BOOST_AUTO_TEST_CASE(read_cached)
{
    struct mock_algo : sansio_algorithm
    {
        coroutine coro;
        std::uint8_t seqnum{};

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return read(seqnum);
                BOOST_TEST(ec == error_code());
                BOOST_TEST(seqnum == 1u);
                BOOST_MYSQL_ASSERT_BUFFER_EQUALS(st_->reader.message(), msg1);
                BOOST_ASIO_CORO_YIELD return read(seqnum);
                BOOST_TEST(ec == error_code());
                BOOST_TEST(seqnum == 2u);
                BOOST_MYSQL_ASSERT_BUFFER_EQUALS(st_->reader.message(), msg2);
            }
            return next_action();
        }
    };

    connection_state_data st(512);
    mock_algo algo(st);
    algo_runner runner(algo);

    // Initial run yields a read request. We don't have cached data, so run_op returns it
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::read);
    BOOST_TEST(act.read_args().buffer.data() == st.reader.buffer().data());
    BOOST_TEST(act.read_args().buffer.size() == st.reader.buffer().size());
    BOOST_TEST(!act.read_args().use_ssl);

    // Acknowledge the read request
    auto bytes = concat_copy(create_frame(0, msg1), create_frame(1, msg2));
    transfer(act.read_args().buffer, bytes);
    act = runner.resume(error_code(), bytes.size());

    // The second read request is acknowledged directly, since it has cached data
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(read_short_and_buffer_resizing)
{
    struct mock_algo : sansio_algorithm
    {
        coroutine coro;
        std::uint8_t seqnum{};

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return read(seqnum);
                BOOST_TEST(ec == error_code());
                BOOST_TEST(seqnum == 1u);
                BOOST_MYSQL_ASSERT_BUFFER_EQUALS(st_->reader.message(), msg2);
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    mock_algo algo(st);
    algo_runner runner(algo);

    // Initial run yields a read request and resizes the buffer aprorpiately
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::read);
    BOOST_TEST(act.read_args().buffer.data() == st.reader.buffer().data());
    BOOST_TEST(act.read_args().buffer.size() == st.reader.buffer().size());
    BOOST_TEST(!act.read_args().use_ssl);

    // Acknowledge the read request. There is space for the header, at least
    auto bytes = create_frame(0, msg2);
    transfer(act.read_args().buffer, span<const std::uint8_t>(bytes.data(), 4));
    act = runner.resume(error_code(), 4);

    // The read request wasn't completely satisified, so more bytes are asked for
    BOOST_TEST(act.type() == next_action::type_t::read);

    // Read part of the body
    transfer(act.read_args().buffer, span<const std::uint8_t>(bytes.data() + 4, 10));
    act = runner.resume(error_code(), 10);
    BOOST_TEST(act.type() == next_action::type_t::read);

    // Complete
    transfer(act.read_args().buffer, span<const std::uint8_t>(bytes.data() + 14, bytes.size() - 14));
    act = runner.resume(error_code(), bytes.size() - 14);
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(read_parsing_error)
{
    struct mock_algo : sansio_algorithm
    {
        coroutine coro;
        std::uint8_t seqnum{42u};

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return read(seqnum);
                BOOST_TEST(ec == error_code(client_errc::sequence_number_mismatch));
            }
            return next_action();
        }
    };

    connection_state_data st(512);
    mock_algo algo(st);
    algo_runner runner(algo);

    // Initial run yields a read request. We don't have cached data, so run_op returns it
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::read);

    // Acknowledge the read request. This causes a seqnum mismatch that is transmitted to the op
    auto bytes = create_frame(0, msg1);
    transfer(act.read_args().buffer, bytes);
    act = runner.resume(error_code(), bytes.size());

    // Op done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(read_io_error)
{
    struct mock_algo : sansio_algorithm
    {
        coroutine coro;
        std::uint8_t seqnum{};

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return read(seqnum);
                BOOST_TEST(ec == error_code(client_errc::wrong_num_params));
            }
            return next_action();
        }
    };

    connection_state_data st(512);
    mock_algo algo(st);
    algo_runner runner(algo);

    // Initial run yields a read request. We don't have cached data, so run_op returns it
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::read);

    // Read request fails with an error
    act = runner.resume(client_errc::wrong_num_params, 0);

    // Op done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(read_ssl_active)
{
    struct mock_algo : sansio_algorithm
    {
        std::uint8_t seqnum{};

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_TEST(ec == error_code());
            return read(seqnum);
        }
    };

    connection_state_data st(512);
    mock_algo algo(st);
    algo_runner runner(algo);
    st.ssl = ssl_state::active;

    // Yielding a read with ssl active sets the use_ssl flag
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::read);
    BOOST_TEST(act.read_args().buffer.data() == st.reader.buffer().data());
    BOOST_TEST(act.read_args().buffer.size() == st.reader.buffer().size());
    BOOST_TEST(act.read_args().use_ssl);
}

BOOST_AUTO_TEST_CASE(write_short)
{
    struct mock_algo : sansio_algorithm
    {
        coroutine coro;
        std::uint8_t seqnum{};

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return write(mock_message{msg1}, seqnum);
                BOOST_TEST(ec == error_code());
                BOOST_TEST(seqnum == 1u);
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    mock_algo algo(st);
    algo_runner runner(algo);

    // Initial run yields a write request
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::write);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(act.write_args().buffer, create_frame(0, msg1));
    BOOST_TEST(!act.write_args().use_ssl);

    // Acknowledge part of the write. This will ask for more bytes to be written
    act = runner.resume(error_code(), 4);
    BOOST_TEST(act.type() == next_action::type_t::write);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(act.write_args().buffer, msg1);

    // Complete
    act = runner.resume(error_code(), 3);
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(write_io_error)
{
    struct mock_algo : sansio_algorithm
    {
        coroutine coro;
        std::uint8_t seqnum{};

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return write(mock_message{msg1}, seqnum);
                BOOST_TEST(ec == error_code(client_errc::wrong_num_params));
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    mock_algo algo(st);
    algo_runner runner(algo);

    // Initial run yields a write request. Fail it
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::write);
    act = runner.resume(client_errc::wrong_num_params, 0);

    // Done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(write_ssl_active)
{
    struct mock_algo : sansio_algorithm
    {
        std::uint8_t seqnum{};

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_TEST(ec == error_code());
            return write(mock_message{msg1}, seqnum);
        }
    };

    connection_state_data st(0);
    mock_algo algo(st);
    algo_runner runner(algo);
    st.ssl = ssl_state::active;

    // Yielding a write request when ssl_active() returns an action with the flag set
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::write);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(act.write_args().buffer, create_frame(0, msg1));
    BOOST_TEST(act.write_args().use_ssl);
}

BOOST_AUTO_TEST_CASE(ssl_handshake)
{
    struct mock_algo : sansio_algorithm
    {
        boost::asio::coroutine coro;

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return next_action::ssl_handshake();
                BOOST_TEST(ec == error_code(client_errc::wrong_num_params));
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    mock_algo algo(st);
    algo_runner runner(algo);

    // Initial run yields a SSL handshake request. These are always returned
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::ssl_handshake);

    // Fail the op
    act = runner.resume(client_errc::wrong_num_params, 0);

    // Done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(ssl_shutdown)
{
    struct mock_algo : sansio_algorithm
    {
        coroutine coro;

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return next_action::ssl_shutdown();
                BOOST_TEST(ec == error_code(client_errc::wrong_num_params));
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    mock_algo algo(st);
    algo_runner runner(algo);

    // Initial run yields a SSL handshake request. These are always returned
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::ssl_shutdown);

    // Fail the op
    act = runner.resume(client_errc::wrong_num_params, 0);

    // Done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(connect)
{
    struct mock_algo : sansio_algorithm
    {
        boost::asio::coroutine coro;

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return next_action::connect();
                BOOST_TEST(ec == error_code(client_errc::wrong_num_params));
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    mock_algo algo(st);
    algo_runner runner(algo);

    // Initial run yields a connect request. These are always returned
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::connect);

    // Fail the op
    act = runner.resume(client_errc::wrong_num_params, 0);

    // Done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(close)
{
    struct mock_algo : sansio_algorithm
    {
        boost::asio::coroutine coro;

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return next_action::close();
                BOOST_TEST(ec == error_code(client_errc::wrong_num_params));
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    mock_algo algo(st);
    algo_runner runner(algo);

    // Initial run yields a close request. These are always returned
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action::type_t::close);

    // Fail the op
    act = runner.resume(client_errc::wrong_num_params, 0);

    // Done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(immediate_completion)
{
    struct mock_algo : sansio_algorithm
    {
        boost::asio::coroutine coro;

        mock_algo(connection_state_data& st) : sansio_algorithm(st) {}

        next_action resume(error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return next_action();
                BOOST_TEST(false);  // Should never be called again after next_action() is returned
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    mock_algo algo(st);
    algo_runner runner(algo);

    // Initial run yields completion
    auto act = runner.resume(error_code(), 0);
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_SUITE_END()
