//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ENGINE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_ENGINE_IMPL_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/any_resumable_ref.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/engine.hpp>
#include <boost/mysql/detail/next_action.hpp>
#include <boost/mysql/detail/stream_adaptor.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/detail/assert.hpp>
#include <boost/asio/post.hpp>
#include <boost/assert.hpp>

#include <cstddef>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

inline asio::mutable_buffer to_buffer(span<std::uint8_t> buff) noexcept
{
    return asio::mutable_buffer(buff.data(), buff.size());
}

template <class EngineStream>
struct run_algo_op : asio::coroutine
{
    EngineStream& stream_;
    any_resumable_ref resumable_;
    bool has_done_io_{false};
    error_code stored_ec_;

    run_algo_op(EngineStream& stream, any_resumable_ref algo) noexcept : stream_(stream), resumable_(algo) {}

    template <class Self>
    void operator()(Self& self, error_code io_ec = {}, std::size_t bytes_transferred = 0)
    {
        next_action act;

        BOOST_ASIO_CORO_REENTER(*this)
        {
            while (true)
            {
                // Run the op
                act = resumable_.resume(io_ec, bytes_transferred);
                if (act.is_done())
                {
                    stored_ec_ = act.error();
                    if (!has_done_io_)
                    {
                        BOOST_ASIO_CORO_YIELD asio::post(stream_.get_executor(), std::move(self));
                    }
                    self.complete(stored_ec_);
                    BOOST_ASIO_CORO_YIELD break;
                }
                else if (act.type() == next_action::type_t::read)
                {
                    BOOST_ASIO_CORO_YIELD stream_.async_read_some(
                        to_buffer(act.read_args().buffer),
                        act.read_args().use_ssl,
                        std::move(self)
                    );
                    has_done_io_ = true;
                }
                else if (act.type() == next_action::type_t::write)
                {
                    BOOST_ASIO_CORO_YIELD stream_.async_write_some(
                        asio::buffer(act.write_args().buffer),
                        act.write_args().use_ssl,
                        std::move(self)
                    );
                    has_done_io_ = true;
                }
                else if (act.type() == next_action::type_t::ssl_handshake)
                {
                    BOOST_ASIO_CORO_YIELD stream_.async_handshake(std::move(self));
                    has_done_io_ = true;
                }
                else if (act.type() == next_action::type_t::ssl_shutdown)
                {
                    BOOST_ASIO_CORO_YIELD stream_.async_shutdown(std::move(self));
                    has_done_io_ = true;
                }
                else if (act.type() == next_action::type_t::connect)
                {
                    BOOST_ASIO_CORO_YIELD stream_.async_connect(std::move(self));
                    has_done_io_ = true;
                }
                else
                {
                    BOOST_ASSERT(act.type() == next_action::type_t::close);
                    stream_.close(io_ec);
                }
            }
        }
    }
};

// stream_adaptor can be used to adapt a traditional Asio stream to an EngineStream
template <class EngineStream>
class engine_impl final : public engine
{
    EngineStream stream_;

public:
    template <class... Args>
    engine_impl(Args&&... args) : stream_(std::forward<Args>(args)...)
    {
    }

    EngineStream& stream() { return stream_; }
    const EngineStream& stream() const { return stream_; }

    using executor_type = asio::any_io_executor;
    executor_type get_executor() override final { return stream_.get_executor(); }

    bool supports_ssl() const override final { return stream_.supports_ssl(); }

    void set_endpoint(const void* endpoint) override final { stream_.set_endpoint(endpoint); }

    void run(any_resumable_ref resumable, error_code& ec) override final
    {
        ec.clear();
        error_code io_ec;
        std::size_t bytes_transferred = 0;

        while (true)
        {
            // Run the op
            auto act = resumable.resume(io_ec, bytes_transferred);

            // Apply the next action
            bytes_transferred = 0;
            if (act.is_done())
            {
                ec = act.error();
                return;
            }
            else if (act.type() == next_action::type_t::read)
            {
                bytes_transferred = stream_.read_some(
                    to_buffer(act.read_args().buffer),
                    act.read_args().use_ssl,
                    io_ec
                );
            }
            else if (act.type() == next_action::type_t::write)
            {
                bytes_transferred = stream_.write_some(
                    asio::buffer(act.write_args().buffer),
                    act.write_args().use_ssl,
                    io_ec
                );
            }
            else if (act.type() == next_action::type_t::ssl_handshake)
            {
                stream_.handshake(io_ec);
            }
            else if (act.type() == next_action::type_t::ssl_shutdown)
            {
                stream_.shutdown(io_ec);
            }
            else if (act.type() == next_action::type_t::connect)
            {
                stream_.connect(io_ec);
            }
            else
            {
                BOOST_ASSERT(act.type() == next_action::type_t::close);
                stream_.close(io_ec);
            }
        }
    }

    void async_run(any_resumable_ref resumable, asio::any_completion_handler<void(error_code)> h)
        override final
    {
        return asio::async_compose<asio::any_completion_handler<void(error_code)>, void(error_code)>(
            run_algo_op<EngineStream>(stream_, resumable),
            h,
            stream_
        );
    }
};

#ifdef BOOST_MYSQL_SEPARATE_COMPILATION
extern template class engine_impl<stream_adaptor<asio::ssl::stream<asio::ip::tcp::socket>>>;
extern template class engine_impl<stream_adaptor<asio::ip::tcp::socket>>;
#endif

template <class Stream, class... Args>
std::unique_ptr<engine> make_engine(Args&&... args)
{
    return std::unique_ptr<engine>(new engine_impl<stream_adaptor<Stream>>(std::forward<Args>(args)...));
}

// Use these only for engines created using make_engine
template <class Stream>
Stream& stream_from_engine(engine& eng)
{
    using derived_t = engine_impl<stream_adaptor<Stream>>;
    return static_cast<derived_t&>(eng).stream().stream();
}

template <class Stream>
const Stream& stream_from_engine(const engine& eng)
{
    using derived_t = engine_impl<stream_adaptor<Stream>>;
    return static_cast<const derived_t&>(eng).stream().stream();
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif