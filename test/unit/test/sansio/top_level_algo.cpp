//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/protocol/frame_header.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/message_reader.hpp>
#include <boost/mysql/impl/internal/sansio/top_level_algo.hpp>

#include <boost/asio/coroutine.hpp>
#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <cstdint>
#include <cstring>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/buffer_concat.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/mock_message.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql::detail;
using namespace boost::mysql::test;
using boost::span;
using boost::asio::coroutine;
using boost::mysql::client_errc;
using boost::mysql::diagnostics;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_top_level_algo)

void transfer(span<std::uint8_t> buff, span<const std::uint8_t> bytes)
{
    assert(buff.size() >= bytes.size());
    std::memcpy(buff.data(), bytes.data(), bytes.size());
}

const std::vector<std::uint8_t> msg1{0x01, 0x02, 0x03};
const std::vector<std::uint8_t> msg2(50, 0x04);

BOOST_AUTO_TEST_CASE(read_cached)
{
    struct mock_algo
    {
        coroutine coro;
        std::uint8_t seqnum{};

        next_action resume(connection_state_data& st, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return st.read(seqnum);
                BOOST_TEST(ec == error_code());
                BOOST_TEST(seqnum == 1u);
                BOOST_MYSQL_ASSERT_BUFFER_EQUALS(st.reader.message(), msg1);
                BOOST_ASIO_CORO_YIELD return st.read(seqnum);
                BOOST_TEST(ec == error_code());
                BOOST_TEST(seqnum == 2u);
                BOOST_MYSQL_ASSERT_BUFFER_EQUALS(st.reader.message(), msg2);
            }
            return next_action();
        }
    };

    connection_state_data st(512);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a read request. We don't have cached data, so run_op returns it
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::read);
    BOOST_TEST(act.read_args().buffer.data() == st.reader.buffer().data());
    BOOST_TEST(act.read_args().buffer.size() == st.reader.buffer().size());
    BOOST_TEST(!act.read_args().use_ssl);

    // Acknowledge the read request
    auto bytes = concat(create_frame(0, msg1), create_frame(1, msg2));
    transfer(act.read_args().buffer, bytes);
    act = algo.resume(error_code(), bytes.size());

    // The second read request is acknowledged directly, since it has cached data
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(read_short_and_buffer_resizing)
{
    struct mock_algo
    {
        coroutine coro;
        std::uint8_t seqnum{};

        next_action resume(connection_state_data& st, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return st.read(seqnum);
                BOOST_TEST(ec == error_code());
                BOOST_TEST(seqnum == 1u);
                BOOST_MYSQL_ASSERT_BUFFER_EQUALS(st.reader.message(), msg2);
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a read request and resizes the buffer appropriately
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::read);
    BOOST_TEST(act.read_args().buffer.data() == st.reader.buffer().data());
    BOOST_TEST(act.read_args().buffer.size() == st.reader.buffer().size());
    BOOST_TEST(!act.read_args().use_ssl);

    // Acknowledge the read request. There is space for the header, at least
    auto bytes = create_frame(0, msg2);
    transfer(act.read_args().buffer, span<const std::uint8_t>(bytes.data(), 4));
    act = algo.resume(error_code(), 4);

    // The read request wasn't completely satisfied, so more bytes are asked for
    BOOST_TEST(act.type() == next_action_type::read);

    // Read part of the body
    transfer(act.read_args().buffer, span<const std::uint8_t>(bytes.data() + 4, 10));
    act = algo.resume(error_code(), 10);
    BOOST_TEST(act.type() == next_action_type::read);

    // Complete
    transfer(act.read_args().buffer, span<const std::uint8_t>(bytes.data() + 14, bytes.size() - 14));
    act = algo.resume(error_code(), bytes.size() - 14);
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(read_parsing_error)
{
    struct mock_algo
    {
        coroutine coro;
        std::uint8_t seqnum{42u};

        next_action resume(connection_state_data& st, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return st.read(seqnum);
                BOOST_TEST(ec == client_errc::sequence_number_mismatch);
            }
            return next_action();
        }
    };

    connection_state_data st(512);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a read request. We don't have cached data, so run_op returns it
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::read);

    // Acknowledge the read request. This causes a seqnum mismatch that is transmitted to the op
    auto bytes = create_frame(0, msg1);
    transfer(act.read_args().buffer, bytes);
    act = algo.resume(error_code(), bytes.size());

    // Op done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(read_io_error)
{
    struct mock_algo
    {
        coroutine coro;
        std::uint8_t seqnum{};

        next_action resume(connection_state_data& st, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return st.read(seqnum);
                BOOST_TEST(ec == client_errc::wrong_num_params);
            }
            return next_action();
        }
    };

    connection_state_data st(512);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a read request. We don't have cached data, so run_op returns it
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::read);

    // Read request fails with an error
    act = algo.resume(client_errc::wrong_num_params, 0);

    // Op done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(read_buffer_size_exceeded)
{
    struct mock_algo
    {
        coroutine coro;
        std::uint8_t seqnum{42u};

        next_action resume(connection_state_data& st, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return st.read(seqnum);
                BOOST_TEST(ec == client_errc::max_buffer_size_exceeded);
            }
            return next_action();
        }
    };

    connection_state_data st(32, 64);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a read request. We don't have cached data, so resume returns it
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::read);

    // Acknowledge the read request. This attempts to resize the buffer past max_size and errors
    std::array<std::uint8_t, 4> header{};
    serialize_frame_header(header, {80, 42});
    transfer(act.read_args().buffer, header);
    act = algo.resume(error_code(), header.size());

    // Op done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(read_ssl_active)
{
    struct mock_algo
    {
        std::uint8_t seqnum{};

        next_action resume(connection_state_data& st, diagnostics&, error_code ec)
        {
            BOOST_TEST(ec == error_code());
            return st.read(seqnum);
        }
    };

    connection_state_data st(512);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);
    st.tls_active = true;

    // Yielding a read with ssl active sets the use_ssl flag
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::read);
    BOOST_TEST(act.read_args().buffer.data() == st.reader.buffer().data());
    BOOST_TEST(act.read_args().buffer.size() == st.reader.buffer().size());
    BOOST_TEST(act.read_args().use_ssl);
}

BOOST_AUTO_TEST_CASE(write_short)
{
    struct mock_algo
    {
        coroutine coro;
        std::uint8_t seqnum{};

        next_action resume(connection_state_data& st, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return st.write(mock_message{msg1}, seqnum);
                BOOST_TEST(ec == error_code());
                BOOST_TEST(seqnum == 1u);
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a write request
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::write);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(act.write_args().buffer, create_frame(0, msg1));
    BOOST_TEST(!act.write_args().use_ssl);

    // Acknowledge part of the write. This will ask for more bytes to be written
    act = algo.resume(error_code(), 4);
    BOOST_TEST(act.type() == next_action_type::write);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(act.write_args().buffer, msg1);

    // Complete
    act = algo.resume(error_code(), 3);
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(write_io_error)
{
    struct mock_algo
    {
        coroutine coro;
        std::uint8_t seqnum{};

        next_action resume(connection_state_data& st, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return st.write(mock_message{msg1}, seqnum);
                BOOST_TEST(ec == client_errc::wrong_num_params);
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a write request. Fail it
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::write);
    act = algo.resume(client_errc::wrong_num_params, 0);

    // Done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(write_max_buffer_size_exact)
{
    struct mock_algo
    {
        coroutine coro;
        std::uint8_t seqnum{};
        const std::array<std::uint8_t, 60> long_msg{};

        next_action resume(connection_state_data& st, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return st.write(mock_message{long_msg}, seqnum);
                BOOST_TEST(ec == error_code());
            }
            return next_action();
        }
    };

    connection_state_data st(32, 64);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a write request, exactly of max_size. This succeeds
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::write);
    BOOST_TEST(act.write_args().buffer.size() == 64u);
    act = algo.resume(error_code(), 64);

    // Done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(write_ssl_active)
{
    struct mock_algo
    {
        std::uint8_t seqnum{};

        next_action resume(connection_state_data& st, diagnostics&, error_code ec)
        {
            BOOST_TEST(ec == error_code());
            return st.write(mock_message{msg1}, seqnum);
        }
    };

    connection_state_data st(0);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);
    st.tls_active = true;

    // Yielding a write request when ssl_active() returns an action with the flag set
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::write);
    BOOST_MYSQL_ASSERT_BUFFER_EQUALS(act.write_args().buffer, create_frame(0, msg1));
    BOOST_TEST(act.write_args().use_ssl);
}

BOOST_AUTO_TEST_CASE(ssl_handshake)
{
    struct mock_algo
    {
        boost::asio::coroutine coro;

        next_action resume(connection_state_data&, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return next_action::ssl_handshake();
                BOOST_TEST(ec == client_errc::wrong_num_params);
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a SSL handshake request. These are always returned
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::ssl_handshake);

    // Fail the op
    act = algo.resume(client_errc::wrong_num_params, 0);

    // Done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(ssl_shutdown)
{
    struct mock_algo
    {
        coroutine coro;

        next_action resume(connection_state_data&, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return next_action::ssl_shutdown();
                BOOST_TEST(ec == client_errc::wrong_num_params);
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a SSL handshake request. These are always returned
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::ssl_shutdown);

    // Fail the op
    act = algo.resume(client_errc::wrong_num_params, 0);

    // Done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(connect)
{
    struct mock_algo
    {
        boost::asio::coroutine coro;

        next_action resume(connection_state_data&, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return next_action::connect(reinterpret_cast<const void*>(42u));
                BOOST_TEST(ec == client_errc::wrong_num_params);
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a connect request. These are always returned.
    // The connect argument is not used, only forwarded
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::connect);
    BOOST_TEST(act.connect_endpoint() == reinterpret_cast<const void*>(42u));

    // Fail the op
    act = algo.resume(client_errc::wrong_num_params, 0);

    // Done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(close)
{
    struct mock_algo
    {
        boost::asio::coroutine coro;

        next_action resume(connection_state_data&, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(ec == error_code());
                BOOST_ASIO_CORO_YIELD return next_action::close();
                BOOST_TEST(ec == client_errc::wrong_num_params);
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields a close request. These are always returned
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::close);

    // Fail the op
    act = algo.resume(client_errc::wrong_num_params, 0);

    // Done
    BOOST_TEST(act.success());
}

BOOST_AUTO_TEST_CASE(immediate_completion)
{
    struct mock_algo
    {
        boost::asio::coroutine coro;

        next_action resume(connection_state_data&, diagnostics&, error_code ec)
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
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initial run yields completion
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.success());
}

// Diagnostics are correctly passed around
BOOST_AUTO_TEST_CASE(diagnostics_passed)
{
    struct mock_algo
    {
        coroutine coro;
        const diagnostics* expected_diag;

        explicit mock_algo(const diagnostics* expected_diag) : expected_diag(expected_diag) {}

        next_action resume(connection_state_data&, diagnostics& diag, error_code)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                // Diagnostics passed by reference
                BOOST_TEST(&diag == expected_diag);

                // We can set the output value
                diag = create_server_diag("abc");
                BOOST_ASIO_CORO_YIELD return next_action::ssl_handshake();

                // The same object is passed again
                BOOST_TEST(&diag == expected_diag);
            }
            return next_action();
        }
    };

    connection_state_data st(512);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag, &diag);

    // Initiate
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::ssl_handshake);

    // We can see the diagnostics set by the algorithm
    BOOST_TEST(diag == create_server_diag("abc"));

    // Continue
    act = algo.resume(error_code(), 0);
    BOOST_TEST(act.success());

    // Diagnostics still the same
    BOOST_TEST(diag == create_server_diag("abc"));
}

// The in-progress flag is set and cleared
BOOST_AUTO_TEST_CASE(in_progress_flag_success)
{
    struct mock_algo
    {
        boost::asio::coroutine coro;

        next_action resume(connection_state_data& st, diagnostics&, error_code)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(st.op_in_progress);  // always set when running an algo
                BOOST_ASIO_CORO_YIELD return next_action::ssl_handshake();
                BOOST_TEST(st.op_in_progress);  // always set when running an algo
            }
            return next_action();
        }
    };

    connection_state_data st(0);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initiating sets the flag
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::ssl_handshake);
    BOOST_TEST(st.op_in_progress);

    // Finishing clears it
    act = algo.resume(error_code(), 0);
    BOOST_TEST(act.success());
    BOOST_TEST(!st.op_in_progress);
}

BOOST_AUTO_TEST_CASE(in_progress_flag_error)
{
    struct mock_algo
    {
        boost::asio::coroutine coro;

        next_action resume(connection_state_data& st, diagnostics&, error_code ec)
        {
            BOOST_ASIO_CORO_REENTER(coro)
            {
                BOOST_TEST(st.op_in_progress);  // always set when running an algo
                BOOST_ASIO_CORO_YIELD return next_action::ssl_handshake();
                BOOST_TEST(st.op_in_progress);  // always set when running an algo
            }
            return ec;  // return the error passed to the algo
        }
    };

    connection_state_data st(0);
    diagnostics diag;
    top_level_algo<mock_algo> algo(st, diag);

    // Initiating sets the flag
    auto act = algo.resume(error_code(), 0);
    BOOST_TEST(act.type() == next_action_type::ssl_handshake);
    BOOST_TEST(st.op_in_progress);

    // Finishing clears it, even if there was an error either in the algo itself
    // or in the code managed by top_level_algo (e.g. a network error)
    act = algo.resume(client_errc::sequence_number_mismatch, 0);
    BOOST_TEST(act.error() == client_errc::sequence_number_mismatch);
    BOOST_TEST(!st.op_in_progress);
}

BOOST_AUTO_TEST_SUITE_END()
