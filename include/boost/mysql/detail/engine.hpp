//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ENGINE_HPP
#define BOOST_MYSQL_DETAIL_ENGINE_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/any_resumable_ref.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/capy/buffers.hpp>
#include <boost/capy/buffers/make_buffer.hpp>
#include <boost/capy/io/any_stream.hpp>
#include <boost/capy/io_task.hpp>

#include <cstddef>
#include <memory>

namespace boost {
namespace mysql {
namespace detail {

class mysql_stream : public capy::any_stream
{
    bool supports_tls_;

public:
    explicit mysql_stream(bool supports_tls) : supports_tls_(supports_tls) {}
    virtual ~mysql_stream() {}
    virtual capy::io_task<std::size_t> read_some(capy::mutable_buffer) = 0;
    virtual capy::io_task<std::size_t> write_some(capy::const_buffer) = 0;
    virtual capy::io_task<> connect(const any_address&) = 0;
    virtual capy::io_task<> tls_handshake() = 0;
    virtual capy::io_task<> tls_shutdown() = 0;
    virtual capy::io_task<> close() = 0;

    bool supports_tls() const { return supports_tls_; }
};

class engine
{
    std::unique_ptr<mysql_stream> stream_;

public:
    engine(std::unique_ptr<mysql_stream> stream) noexcept : stream_(std::move(stream)) {}
    ~engine() {}
    capy::io_task<> run(any_resumable_ref resumable)
    {
        // Start the operation
        auto act = resumable.resume(error_code(), 0u);

        while (true)
        {
            switch (act.type())
            {
            case next_action_type::none: co_return {act.error()};
            case next_action_type::read:
            {
                auto [ec, n] = co_await stream_->read_some(capy::make_buffer(act.read_args().buffer));
                act = resumable.resume(ec, n);
                break;
            }
            case next_action_type::write:
            {
                auto [ec, n] = co_await stream_->write_some(capy::make_buffer(act.write_args().buffer));
                act = resumable.resume(ec, n);
                break;
            }
            case next_action_type::ssl_handshake:
            {
                auto [ec] = co_await stream_->tls_handshake();
                act = resumable.resume(ec, 0u);
                break;
            }
            case next_action_type::ssl_shutdown:
            {
                auto [ec] = co_await stream_->tls_shutdown();
                act = resumable.resume(ec, 0u);
                break;
            }
            case next_action_type::connect:
            {
                auto [ec] = co_await stream_->connect(act.connect_endpoint());
                act = resumable.resume(ec, 0u);
                break;
            }
            case next_action_type::close:
            {
                auto [ec] = co_await stream_->close();
                act = resumable.resume(ec, 0u);
                break;
            }
            default: BOOST_ASSERT(false); co_return {};
            }
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
