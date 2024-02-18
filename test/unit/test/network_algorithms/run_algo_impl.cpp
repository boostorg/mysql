//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/any_stream.hpp>

#include <boost/mysql/impl/internal/network_algorithms/run_algo_impl.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/next_action.hpp>
#include <boost/mysql/impl/internal/sansio/sansio_algorithm.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "test_common/netfun_maker.hpp"
#include "test_common/tracker_executor.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/mock_message.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql::detail;
namespace asio = boost::asio;
using boost::mysql::error_code;

BOOST_AUTO_TEST_SUITE(test_run_algo_impl)

using netfun_maker = netfun_maker_fn<void, any_stream&, any_algo_ref>;

const auto sync_fn = netfun_maker::sync_errc_noerrinfo(&run_algo_impl);
const auto async_fn = netfun_maker::async_noerrinfo(&async_run_algo_impl);

void complete_immediate(
    asio::any_io_executor ex,
    asio::any_completion_handler<void(error_code)> h,
    error_code ec
)
{
    asio::post(ex, asio::deferred([ec]() { return asio::deferred.values(ec); }))(std::move(h));
}

void complete_immediate(
    asio::any_io_executor ex,
    asio::any_completion_handler<void(error_code, std::size_t)> h,
    error_code ec,
    std::size_t bytes
)
{
    asio::post(ex, asio::deferred([ec, bytes]() { return asio::deferred.values(ec, bytes); }))(std::move(h));
}

class mock_stream final : public any_stream
{
    executor_info stream_executor_info_;
    asio::any_io_executor ex;
    static std::size_t transfer_empty_frame(asio::mutable_buffer buff)
    {
        auto empty_frame = create_empty_frame(0);
        assert(buff.size() >= empty_frame.size());
        std::memcpy(buff.data(), empty_frame.data(), empty_frame.size());
        return empty_frame.size();
    }

public:
    std::vector<next_action> calls;

    mock_stream(asio::any_io_executor ex)
        : any_stream(true), ex(create_tracker_executor(ex, &stream_executor_info_))
    {
    }

    executor_type get_executor() override { return ex; }

    // SSL
    void handshake(error_code& ec) override
    {
        calls.push_back(next_action::ssl_handshake());
        ec = error_code();
    }
    void async_handshake(asio::any_completion_handler<void(error_code)> h) override
    {
        calls.push_back(next_action::ssl_handshake());
        complete_immediate(ex, std::move(h), error_code());
    }
    void shutdown(error_code& ec) override
    {
        calls.push_back(next_action::ssl_shutdown());
        ec = error_code();
    }
    void async_shutdown(asio::any_completion_handler<void(error_code)> h) override
    {
        calls.push_back(next_action::ssl_shutdown());
        complete_immediate(ex, std::move(h), error_code());
    }

    // Reading
    std::size_t read_some(asio::mutable_buffer buff, bool use_ssl, error_code& ec) override
    {
        calls.push_back(next_action::read({{}, use_ssl}));
        ec = error_code();
        return transfer_empty_frame(buff);
    }
    void async_read_some(
        asio::mutable_buffer buff,
        bool use_ssl,
        asio::any_completion_handler<void(error_code, std::size_t)> h
    ) override
    {
        calls.push_back(next_action::read({{}, use_ssl}));
        complete_immediate(ex, std::move(h), error_code(), transfer_empty_frame(buff));
    }

    // Writing
    std::size_t write_some(asio::const_buffer buff, bool use_ssl, error_code& ec) override
    {
        calls.push_back(next_action::write({{}, use_ssl}));
        ec = {};
        return buff.size();
    }
    void async_write_some(
        asio::const_buffer buff,
        bool use_ssl,
        asio::any_completion_handler<void(error_code, std::size_t)> h
    ) override
    {
        calls.push_back(next_action::write({{}, use_ssl}));
        complete_immediate(ex, std::move(h), error_code(), buff.size());
    }

    // Connect and close
    void set_endpoint(const void*) override {}
    void connect(error_code& ec) override
    {
        calls.push_back(next_action::connect());
        ec = error_code();
    }
    void async_connect(asio::any_completion_handler<void(error_code)> h) override
    {
        calls.push_back(next_action::connect());
        complete_immediate(ex, std::move(h), error_code());
    }
    void close(error_code& ec) override
    {
        calls.push_back(next_action::close());
        ec = error_code();
    }
};

struct mock_algo : sansio_algorithm, asio::coroutine
{
    std::uint8_t seqnum{};
    next_action act;

    mock_algo(connection_state_data& st, next_action act) : sansio_algorithm(st), act(act) {}

    next_action resume(error_code ec)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_TEST(ec == error_code());
            st_->reader.prepare_read(seqnum);
            st_->writer.prepare_write(mock_message{}, seqnum);
            BOOST_ASIO_CORO_YIELD return act;
        }
        return next_action();
    }
};

// Verify that we correctly post for immediate completions,
// and we don't do extra posts if we've done I/O
BOOST_AUTO_TEST_CASE(async_completions)
{
    struct
    {
        const char* name;
        next_action act;
    } test_cases[] = {
        {"no_action",    next_action()               },
        {"read",         next_action::read({})       },
        {"write",        next_action::write({})      },
        {"ssl_hanshake", next_action::ssl_handshake()},
        {"ssl_shutdown", next_action::ssl_shutdown() },
        {"connect",      next_action::connect()      },
        {"close",        next_action::close()        },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            connection_state_data st(512);
            mock_algo algo(st, tc.act);
            asio::io_context ctx;
            mock_stream stream(ctx.get_executor());

            // Run the algo. In all cases, the stream's executor should receive one post,
            // and the token's executor should receive one dispatch. This all gets
            // validated by async_fn
            async_fn(stream, algo).validate_no_error();
        }
    }
}

BOOST_AUTO_TEST_CASE(read_ssl)
{
    struct
    {
        const char* name;
        netfun_maker::signature fn;
        ssl_state ssl_st;
        bool expected;
    } test_cases[] = {
        {"sync_active",    sync_fn,  ssl_state::active,   true },
        {"sync_inactive",  sync_fn,  ssl_state::inactive, false},
        {"async_active",   async_fn, ssl_state::active,   true },
        {"async_inactive", async_fn, ssl_state::inactive, false},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            connection_state_data st(512);
            st.ssl = tc.ssl_st;
            mock_algo algo(st, next_action::read({}));
            asio::io_context ctx;
            mock_stream stream(ctx.get_executor());

            tc.fn(stream, algo).validate_no_error();
            BOOST_TEST(stream.calls.size() == 1u);
            BOOST_TEST(stream.calls[0].type() == next_action::type_t::read);
            BOOST_TEST(stream.calls[0].read_args().use_ssl == tc.expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(write_ssl)
{
    struct
    {
        const char* name;
        netfun_maker::signature fn;
        ssl_state ssl_st;
        bool expected;
    } test_cases[] = {
        {"sync_active",    sync_fn,  ssl_state::active,   true },
        {"sync_inactive",  sync_fn,  ssl_state::inactive, false},
        {"async_active",   async_fn, ssl_state::active,   true },
        {"async_inactive", async_fn, ssl_state::inactive, false},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            // Setup
            connection_state_data st(512);
            st.ssl = tc.ssl_st;
            mock_algo algo(st, next_action::write({}));
            asio::io_context ctx;
            mock_stream stream(ctx.get_executor());

            tc.fn(stream, algo).validate_no_error();
            BOOST_TEST(stream.calls.size() == 1u);
            BOOST_TEST(stream.calls[0].type() == next_action::type_t::write);
            BOOST_TEST(stream.calls[0].write_args().use_ssl == tc.expected);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
