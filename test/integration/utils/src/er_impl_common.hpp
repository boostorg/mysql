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
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement_base.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <type_traits>

#include "er_connection.hpp"
#include "er_network_variant.hpp"
#include "er_statement.hpp"
#include "get_endpoint.hpp"
#include "streams.hpp"

namespace boost {
namespace mysql {
namespace test {

// Variants
void add_sync_errc(std::vector<er_network_variant*>&);
void add_sync_exc(std::vector<er_network_variant*>&);
void add_async_callback(std::vector<er_network_variant*>&);
void add_async_coroutines(std::vector<er_network_variant*>&);
void add_async_coroutinescpp20(std::vector<er_network_variant*>&);

// Function table
template <class Stream>
struct function_table
{
    using stmt_type = statement<Stream>;
    using conn_type = connection<Stream>;

    using stmt_execute_tuple2_sig =
        network_result<void>(stmt_type&, const std::tuple<field_view, field_view>&, resultset&);
    using stmt_start_execution_tuple2_sig =
        network_result<void>(stmt_type&, const std::tuple<field_view, field_view>&, execution_state&);
    using stmt_start_execution_it_sig =
        network_result<void>(stmt_type&, value_list_it, value_list_it, execution_state&);
    using stmt_close_sig = network_result<void>(stmt_type&);

    using conn_connect_sig = network_result<
        void>(conn_type&, const typename Stream::lowest_layer_type::endpoint_type&, const handshake_params&);
    using conn_handshake_sig = network_result<void>(conn_type&, const handshake_params&);
    using conn_query_sig = network_result<void>(conn_type&, string_view, resultset&);
    using conn_start_query_sig = network_result<void>(conn_type&, string_view, execution_state&);
    using conn_prepare_statement_sig = network_result<void>(conn_type&, string_view, stmt_type&);
    using conn_read_one_row_sig = network_result<row_view>(conn_type&, execution_state&);
    using conn_read_some_rows_sig = network_result<rows_view>(conn_type&, execution_state&);
    using conn_ping_sig = network_result<void>(conn_type&);
    using conn_quit_sig = network_result<void>(conn_type&);
    using conn_close_sig = network_result<void>(conn_type&);

    std::function<stmt_execute_tuple2_sig> stmt_execute_tuple2;
    std::function<stmt_start_execution_tuple2_sig> stmt_start_execution_tuple2;
    std::function<stmt_start_execution_it_sig> stmt_start_execution_it;
    std::function<stmt_close_sig> stmt_close;

    std::function<conn_connect_sig> conn_connect;
    std::function<conn_handshake_sig> conn_handshake;
    std::function<conn_query_sig> conn_query;
    std::function<conn_start_query_sig> conn_start_query;
    std::function<conn_prepare_statement_sig> conn_prepare_statement;
    std::function<conn_read_one_row_sig> conn_read_one_row;
    std::function<conn_read_some_rows_sig> conn_read_some_rows;
    std::function<conn_ping_sig> conn_ping;
    std::function<conn_quit_sig> conn_quit;
    std::function<conn_close_sig> conn_close;
};

// Note: Netmaker should be a struct with a
// public template type called "type", accepting the signature
// arguments as template parameters, and having a static call that
// takes function pointers and returns a type-erased function
// Generate the function table given a sync maker
template <class Stream, class Netmaker>
function_table<Stream> create_sync_table()
{
    using stmt_type = statement<Stream>;
    using conn_type = connection<Stream>;
    using table_t = function_table<Stream>;

    // clang-format off
    return function_table<Stream>{
        Netmaker::template type<typename table_t::stmt_execute_tuple2_sig>::call(&stmt_type::execute),
        Netmaker::template type<typename table_t::stmt_start_execution_tuple2_sig>::call(&stmt_type::start_execution),
        Netmaker::template type<typename table_t::stmt_start_execution_it_sig>::call(&stmt_type::start_execution),
        Netmaker::template type<typename table_t::stmt_close_sig>::call(&stmt_type::close),

        Netmaker::template type<typename table_t::conn_connect_sig>::call(&conn_type::connect),
        Netmaker::template type<typename table_t::conn_handshake_sig>::call(&conn_type::handshake),
        Netmaker::template type<typename table_t::conn_query_sig>::call(&conn_type::query),
        Netmaker::template type<typename table_t::conn_start_query_sig>::call(&conn_type::start_query),
        Netmaker::template type<typename table_t::conn_prepare_statement_sig>::call(&conn_type::prepare_statement),
        Netmaker::template type<typename table_t::conn_read_one_row_sig>::call(&conn_type::read_one_row),
        Netmaker::template type<typename table_t::conn_read_some_rows_sig>::call(&conn_type::read_some_rows),
        Netmaker::template type<typename table_t::conn_ping_sig>::call(&conn_type::ping),
        Netmaker::template type<typename table_t::conn_quit_sig>::call(&conn_type::quit),
        Netmaker::template type<typename table_t::conn_close_sig>::call(&conn_type::close),
    };
    // clang-format on
}

// Generate the function table given an async maker
template <class Stream, class Netmaker>
function_table<Stream> create_async_table()
{
    using stmt_type = statement<Stream>;
    using conn_type = connection<Stream>;
    using table_t = function_table<Stream>;

    // clang-format off
    return function_table<Stream>{
        Netmaker::template type<typename table_t::stmt_execute_tuple2_sig>::call(&stmt_type::async_execute),
        Netmaker::template type<typename table_t::stmt_start_execution_tuple2_sig>::call(&stmt_type::async_start_execution),
        Netmaker::template type<typename table_t::stmt_start_execution_it_sig>::call(&stmt_type::async_start_execution),
        Netmaker::template type<typename table_t::stmt_close_sig>::call(&stmt_type::async_close),

        Netmaker::template type<typename table_t::conn_connect_sig>::call(&conn_type::async_connect),
        Netmaker::template type<typename table_t::conn_handshake_sig>::call(&conn_type::async_handshake),
        Netmaker::template type<typename table_t::conn_query_sig>::call(&conn_type::async_query),
        Netmaker::template type<typename table_t::conn_start_query_sig>::call(&conn_type::async_start_query),
        Netmaker::template type<typename table_t::conn_prepare_statement_sig>::call(&conn_type::async_prepare_statement),
        Netmaker::template type<typename table_t::conn_read_one_row_sig>::call(&conn_type::async_read_one_row),
        Netmaker::template type<typename table_t::conn_read_some_rows_sig>::call(&conn_type::async_read_some_rows),
        Netmaker::template type<typename table_t::conn_ping_sig>::call(&conn_type::async_ping),
        Netmaker::template type<typename table_t::conn_quit_sig>::call(&conn_type::async_quit),
        Netmaker::template type<typename table_t::conn_close_sig>::call(&conn_type::async_close),
    };
    // clang-format on
}

// Variant implementations.
template <class Stream>
class er_statement_impl : public er_statement
{
    const function_table<Stream>& table_;
    using stmt_type = statement<Stream>;
    stmt_type stmt_;

public:
    er_statement_impl(const function_table<Stream>& table) : table_(table) {}
    const statement_base& base() const noexcept override { return stmt_; }
    stmt_type& obj() noexcept { return stmt_; }

    network_result<void> execute_tuple2(field_view param1, field_view param2, resultset& result) override
    {
        return table_.stmt_execute_tuple2(stmt_, std::make_tuple(param1, param2), result);
    }
    network_result<void> start_execution_tuple2(field_view param1, field_view param2, execution_state& st)
        override
    {
        return table_.stmt_start_execution_tuple2(stmt_, std::make_tuple(param1, param2), st);
    }
    network_result<void> start_execution_it(
        value_list_it params_first,
        value_list_it params_last,
        execution_state& st
    ) override
    {
        return table_.stmt_start_execution_it(stmt_, params_first, params_last, st);
    }
    network_result<void> close() override { return table_.stmt_close(stmt_); }
};

template <class Stream>
connection<Stream> create_connection_impl(
    boost::asio::any_io_executor executor,
    boost::asio::ssl::context& ssl_ctx,
    std::true_type  // is ssl stream
)
{
    return connection<Stream>(executor, ssl_ctx);
}

template <class Stream>
connection<Stream> create_connection_impl(
    boost::asio::any_io_executor executor,
    boost::asio::ssl::context&,
    std::false_type  // is ssl stream
)
{
    return connection<Stream>(executor);
}

template <class Stream>
connection<Stream> create_connection(
    boost::asio::any_io_executor executor,
    boost::asio::ssl::context& ssl_ctx
)
{
    return create_connection_impl<Stream>(
        executor,
        ssl_ctx,
        std::integral_constant<bool, supports_ssl<Stream>()>()
    );
}

template <class Stream>
class er_connection_impl : public er_connection
{
    using conn_type = connection<Stream>;

    conn_type conn_;
    er_network_variant& var_;
    const function_table<Stream>& table_;

public:
    er_connection_impl(
        boost::asio::any_io_executor executor,
        boost::asio::ssl::context& ssl_ctx,
        er_network_variant& var,
        const function_table<Stream>& table
    )
        : conn_(create_connection<Stream>(executor, ssl_ctx)), var_(var), table_(table)
    {
    }

    bool uses_ssl() const override { return conn_.uses_ssl(); }
    bool is_open() const override { return conn_.stream().lowest_layer().is_open(); }
    void set_metadata_mode(metadata_mode v) override { conn_.set_meta_mode(v); }
    void physical_connect() override { conn_.stream().lowest_layer().connect(get_endpoint<Stream>()); }
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

    network_result<void> connect(const handshake_params& params) override
    {
        return table_.conn_connect(conn_, get_endpoint<Stream>(), params);
    }
    network_result<void> handshake(const handshake_params& params) override
    {
        return table_.conn_handshake(conn_, params);
    }
    network_result<void> query(string_view query, resultset& result) override
    {
        return table_.conn_query(conn_, query, result);
    }
    network_result<void> start_query(string_view query, execution_state& st) override
    {
        return table_.conn_start_query(conn_, query, st);
    }
    network_result<void> prepare_statement(string_view stmt_sql, er_statement& output_stmt) override
    {
        auto& typed_stmt = static_cast<er_statement_impl<Stream>&>(output_stmt).obj();
        return table_.conn_prepare_statement(conn_, stmt_sql, typed_stmt);
    }
    network_result<row_view> read_one_row(execution_state& st) override
    {
        return table_.conn_read_one_row(conn_, st);
    }
    network_result<rows_view> read_some_rows(execution_state& st) override
    {
        return table_.conn_read_some_rows(conn_, st);
    }
    network_result<void> ping() override { return table_.conn_ping(conn_); }
    network_result<void> quit() override { return table_.conn_quit(conn_); }
    network_result<void> close() override { return table_.conn_close(conn_); }
};

template <class Stream>
class er_network_variant_impl : public er_network_variant
{
    function_table<Stream> table_;
    const char* variant_name_;

    using conn_type = er_connection_impl<Stream>;
    using stmt_type = er_statement_impl<Stream>;

public:
    er_network_variant_impl(function_table<Stream>&& table, const char* name)
        : table_(std::move(table)), variant_name_(name)
    {
    }

    bool supports_ssl() const override { return ::boost::mysql::test::supports_ssl<Stream>(); }
    bool is_unix_socket() const override { return ::boost::mysql::test::is_unix_socket<Stream>(); }
    const char* stream_name() const override { return ::boost::mysql::test::get_stream_name<Stream>(); }
    const char* variant_name() const override { return variant_name_; }
    er_connection_ptr create_connection(boost::asio::any_io_executor ex, boost::asio::ssl::context& ssl_ctx)
        override
    {
        return er_connection_ptr(new conn_type(ex, ssl_ctx, *this, table_));
    }
    er_statement_ptr create_statement() override { return er_statement_ptr(new stmt_type(table_)); }
};

template <class Stream, class Netmaker>
er_network_variant_impl<Stream> create_sync_variant()
{
    return er_network_variant_impl<Stream>(create_sync_table<Stream, Netmaker>(), Netmaker::name());
}

template <class Stream, class Netmaker>
er_network_variant_impl<Stream> create_async_variant()
{
    return er_network_variant_impl<Stream>(create_async_table<Stream, Netmaker>(), Netmaker::name());
}

// Helpers
inline boost::asio::io_context& get_context(boost::asio::any_io_executor ex) noexcept
{
    return static_cast<boost::asio::io_context&>(ex.context());
}

inline void run_until_completion(boost::asio::any_io_executor ex)
{
    auto& ctx = get_context(ex);
    ctx.restart();
    ctx.run();
}

inline void rethrow_on_failure(std::exception_ptr ptr)
{
    if (ptr)
    {
        std::rethrow_exception(ptr);
    }
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
