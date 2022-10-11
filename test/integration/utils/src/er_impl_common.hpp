//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_SRC_ER_IMPL_COMMON_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_SRC_ER_IMPL_COMMON_HPP

#include <boost/mysql/error.hpp>
#include <boost/mysql/statement_base.hpp>
#include "er_connection.hpp"
#include "er_network_variant.hpp"
#include "er_resultset.hpp"
#include "er_statement.hpp"
#include "streams.hpp"
#include <boost/mysql/connection.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/io_context.hpp>
#include <type_traits>

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
    std::true_type // is ssl stream
)
{
    return connection<Stream>(executor, ssl_ctx);
}

template <class Stream>
connection<Stream> create_connection_impl(
    boost::asio::io_context::executor_type executor,
    boost::asio::ssl::context&,
    std::false_type // is ssl stream
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
class er_resultset_base : public er_resultset
{
protected:
    resultset_base<Stream> r_;
public:
    er_resultset_base(resultset_base<Stream>&& r): r_(std::move(r)) {}
    bool valid() const override { return r_.valid(); } 
    bool complete() const override { return r_.complete(); }
    const std::vector<metadata>& fields() const override { return r_.fields(); }
    std::uint64_t affected_rows() const override { return r_.affected_rows(); }
    std::uint64_t last_insert_id() const override { return r_.last_insert_id(); }
    unsigned warning_count() const override { return r_.warning_count(); }
    boost::string_view info() const override { return r_.info(); }
};

template <class Stream>
class er_statement_base : public er_statement
{
protected:
    statement_base<Stream> stmt_;
public:
    er_statement_base(statement_base<Stream>&& stmt): stmt_(std::move(stmt)) {}
    bool valid() const override { return stmt_.valid(); }
    unsigned id() const override { return stmt_.id(); }
    unsigned num_params() const override { return stmt_.num_params(); }
};

template <class Stream>
class er_connection_base : public er_connection
{
protected:
    connection<Stream> conn_;
public:
    er_connection_base(connection<Stream>&& conn): conn_(std::move(conn)) {}
    er_connection_base(
        boost::asio::io_context::executor_type executor,
        boost::asio::ssl::context& ssl_ctx
    ) : conn_(create_connection<Stream>(executor, ssl_ctx))
    {
    }
    bool valid() const override { return conn_.valid(); }
    bool uses_ssl() const override { return conn_.uses_ssl(); }
    bool is_open() const override { return conn_.next_layer().lowest_layer().is_open(); }
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
};

template <class Stream, template <typename> class ConnectionImpl>
class er_network_variant_base : public er_network_variant
{
public:
    bool supports_ssl() const override { return ::boost::mysql::test::supports_ssl<Stream>(); }
    const char* stream_name() const override { return ::boost::mysql::test::get_stream_name<Stream>(); }
    er_connection_ptr create(boost::asio::io_context::executor_type ex, boost::asio::ssl::context& ssl_ctx) override
    {
        return er_connection_ptr(new ConnectionImpl<Stream>(ex, ssl_ctx));
    }
};

// Helpers to erase type
template <template <typename> class ErPreparedStatement, class Stream>
er_statement_ptr erase_statement(statement_base<Stream>&& stmt)
{
    return er_statement_ptr(new ErPreparedStatement<Stream>(std::move(stmt)));
}

template <template <typename> class ErResultset, class Stream>
er_resultset_ptr erase_resultset(resultset_base<Stream>&& r)
{
    return er_resultset_ptr(new ErResultset<Stream>(std::move(r)));
}

} // test
} // mysql
} // boost


#endif
