//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_SRC_ER_IMPL_COMMON_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_SRC_ER_IMPL_COMMON_HPP

#include <boost/mysql/connection.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/statement_base.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <future>
#include <stdexcept>
#include <type_traits>

#include "er_connection.hpp"
#include "er_network_variant.hpp"
#include "er_statement.hpp"
#include "streams.hpp"

namespace boost {
namespace mysql {
namespace test {

// Variants
void add_sync_errc(std::vector<er_network_variant*>&);
void add_sync_exc(std::vector<er_network_variant*>&);
void add_async_callback(std::vector<er_network_variant*>&);
void add_async_callback_noerrinfo(std::vector<er_network_variant*>&);
void add_async_future(std::vector<er_network_variant*>&);
void add_async_coroutine(std::vector<er_network_variant*>&);
void add_async_coroutinecpp20(std::vector<er_network_variant*>&);
void add_default_completion_tokens(std::vector<er_network_variant*>&);

// Helper
template <class Stream>
connection<Stream> create_connection_impl(
    boost::asio::io_context::executor_type executor,
    boost::asio::ssl::context& ssl_ctx,
    std::true_type  // is ssl stream
)
{
    return connection<Stream>(executor, ssl_ctx);
}

template <class Stream>
connection<Stream> create_connection_impl(
    boost::asio::io_context::executor_type executor,
    boost::asio::ssl::context&,
    std::false_type  // is ssl stream
)
{
    return connection<Stream>(executor);
}

template <class Stream>
connection<Stream> create_connection(
    boost::asio::io_context::executor_type executor,
    boost::asio::ssl::context& ssl_ctx
)
{
    return create_connection_impl<Stream>(
        executor,
        ssl_ctx,
        std::integral_constant<bool, supports_ssl<Stream>()>()
    );
}

// Bases to help implement the variants
template <class Stream>
class er_statement_base : public er_statement
{
    statement<Stream> stmt_;

public:
    er_statement_base() = default;
    const statement_base& base() const noexcept override { return stmt_; }
    statement<Stream>& obj() noexcept { return stmt_; }
    const statement<Stream>& obj() const noexcept { return stmt_; }
};

template <class Stream>
class er_connection_base : public er_connection
{
protected:
    connection<Stream> conn_;
    er_network_variant& var_;

public:
    er_connection_base(connection<Stream>&& conn, er_network_variant& var)
        : conn_(std::move(conn)), var_(var)
    {
    }
    er_connection_base(
        boost::asio::io_context::executor_type executor,
        boost::asio::ssl::context& ssl_ctx,
        er_network_variant& var
    )
        : conn_(create_connection<Stream>(executor, ssl_ctx)), var_(var)
    {
    }
    bool uses_ssl() const override { return conn_.uses_ssl(); }
    bool is_open() const override { return conn_.stream().lowest_layer().is_open(); }
    void sync_close() noexcept override
    {
        try
        {
            conn_.close();
        }
        catch (...)
        {
        }
    }
    er_network_variant& variant() const override { return var_; }
    static statement<Stream>& cast(er_statement& stmt) noexcept
    {
        return static_cast<er_statement_base<Stream>&>(stmt).obj();
    }
};

template <
    class Stream,
    template <typename>
    class ConnectionImpl,
    template <typename>
    class StatementImpl>
class er_network_variant_base : public er_network_variant
{
public:
    bool supports_ssl() const override { return ::boost::mysql::test::supports_ssl<Stream>(); }
    bool is_unix_socket() const override { return ::boost::mysql::test::is_unix_socket<Stream>(); }
    const char* stream_name() const override
    {
        return ::boost::mysql::test::get_stream_name<Stream>();
    }
    er_connection_ptr create_connection(
        boost::asio::io_context::executor_type ex,
        boost::asio::ssl::context& ssl_ctx
    ) override
    {
        return er_connection_ptr(new ConnectionImpl<Stream>(ex, ssl_ctx, *this));
    }
    er_statement_ptr create_statement() override
    {
        return er_statement_ptr(new StatementImpl<Stream>());
    }
};

template <typename R>
R wait_for_result(std::future<R>& fut)
{
    auto status = fut.wait_for(std::chrono::seconds(20));
    if (status == std::future_status::timeout)
        throw std::runtime_error("async_callback wait timeout");
    return fut.get();
}

template <typename R>
R wait_for_result(std::promise<R>& prom)
{
    auto fut = prom.get_future();
    return wait_for_result(fut);
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
