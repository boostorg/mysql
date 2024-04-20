//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_SRC_ER_IMPL_COMMON_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_SRC_ER_IMPL_COMMON_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>

#include <cassert>
#include <string>
#include <type_traits>

#include "test_common/ci_server.hpp"
#include "test_common/network_result.hpp"
#include "test_integration/er_connection.hpp"
#include "test_integration/er_network_variant.hpp"
#include "test_integration/get_endpoint.hpp"
#include "test_integration/streams.hpp"

namespace boost {
namespace mysql {
namespace test {

// Variants
void add_sync_errc(std::vector<er_network_variant*>&);
void add_sync_exc(std::vector<er_network_variant*>&);
void add_async_callback(std::vector<er_network_variant*>&);
void add_async_coroutines(std::vector<er_network_variant*>&);
void add_async_coroutinescpp20(std::vector<er_network_variant*>&);

// Helpers
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

// Base for implementing er_connection
template <class Stream>
class connection_base : public er_connection
{
public:
    using stream_type = Stream;
    using conn_type = connection<Stream>;
    using stmt_tuple = bound_statement_tuple<std::tuple<field_view, field_view>>;
    using stmt_it = bound_statement_iterator_range<fv_list_it>;

    connection_base(
        boost::asio::any_io_executor executor,
        boost::asio::ssl::context& ssl_ctx,
        er_network_variant& var
    )
        : conn_(create_connection<Stream>(executor, ssl_ctx)), var_(var)
    {
    }

    conn_type& conn() noexcept { return conn_; }

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

private:
    conn_type conn_;
    er_network_variant& var_;
};

class any_connection_base : public er_connection
{
    static any_connection_params make_ctor_params(asio::ssl::context& ctx) noexcept
    {
        any_connection_params res;
        res.ssl_context = &ctx;
        return res;
    }

public:
    using conn_type = any_connection;
    using base_type = any_connection_base;
    using stmt_tuple = bound_statement_tuple<std::tuple<field_view, field_view>>;
    using stmt_it = bound_statement_iterator_range<fv_list_it>;

    connect_params get_connect_params(const handshake_params& input) const noexcept
    {
        connect_params res;
        if (addr_type_ == address_type::host_and_port)
            res.server_address.emplace_host_and_port(get_hostname());
        else if (addr_type_ == address_type::unix_path)
            res.server_address.emplace_unix_path(default_unix_path);
        res.username = input.username();
        res.password = input.password();
        res.database = input.database();
        res.multi_queries = input.multi_queries();
        return res;
    }

    any_connection_base(
        boost::asio::any_io_executor executor,
        boost::asio::ssl::context& ssl_ctx,
        er_network_variant& var,
        address_type addr
    )
        : conn_(executor, make_ctor_params(ssl_ctx)), var_(var), addr_type_(addr)
    {
    }

    any_connection& conn() noexcept { return conn_; }

    bool uses_ssl() const override { return conn_.uses_ssl(); }
    bool is_open() const override { return true; }  // TODO
    void set_metadata_mode(metadata_mode v) override { conn_.set_meta_mode(v); }
    void physical_connect() override final { assert(false); }  // not allowed in v2
    network_result<void> handshake(const handshake_params&) override final
    {
        return error_code(asio::error::operation_not_supported);
    }
    network_result<void> quit() override final { return error_code(asio::error::operation_not_supported); }

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

private:
    any_connection conn_;
    er_network_variant& var_;
    address_type addr_type_;
};

// Macros to help implementing er_connection
#ifdef BOOST_MYSQL_CXX14
#define BOOST_MYSQL_TEST_IMPLEMENT_GENERIC_CXX14(prefix)                                                  \
    network_result<void> execute(string_view q, er_connection::static_results_t& result) override         \
    {                                                                                                     \
        return this->template fn_impl<void, const string_view&, er_connection::static_results_t&>(        \
            &conn_type::prefix##execute,                                                                  \
            q,                                                                                            \
            result                                                                                        \
        );                                                                                                \
    }                                                                                                     \
    network_result<void> start_execution(string_view q, er_connection::static_state_t& st) override       \
    {                                                                                                     \
        return this->template fn_impl<void, const string_view&, er_connection::static_state_t&>(          \
            &conn_type::prefix##start_execution,                                                          \
            q,                                                                                            \
            st                                                                                            \
        );                                                                                                \
    }                                                                                                     \
    network_result<void> read_resultset_head(er_connection::static_state_t& st) override                  \
    {                                                                                                     \
        return this->template fn_impl<void, er_connection::static_state_t&>(                              \
            &conn_type::prefix##read_resultset_head,                                                      \
            st                                                                                            \
        );                                                                                                \
    }                                                                                                     \
    network_result<std::size_t> read_some_rows(                                                           \
        er_connection::static_state_t& st,                                                                \
        boost::span<row_multifield> storage                                                               \
    ) override                                                                                            \
    {                                                                                                     \
        return this                                                                                       \
            ->template fn_impl<std::size_t, er_connection::static_state_t&, boost::span<row_multifield>>( \
                &conn_type::prefix##read_some_rows,                                                       \
                st,                                                                                       \
                storage                                                                                   \
            );                                                                                            \
    }                                                                                                     \
    network_result<std::size_t> read_some_rows(                                                           \
        er_connection::static_state_t& st,                                                                \
        boost::span<row_2fields> storage                                                                  \
    ) override                                                                                            \
    {                                                                                                     \
        return this                                                                                       \
            ->template fn_impl<std::size_t, er_connection::static_state_t&, boost::span<row_2fields>>(    \
                &conn_type::prefix##read_some_rows,                                                       \
                st,                                                                                       \
                storage                                                                                   \
            );                                                                                            \
    }
#else
#define BOOST_MYSQL_TEST_IMPLEMENT_GENERIC_CXX14(prefix)
#endif

#define BOOST_MYSQL_TEST_IMPLEMENT_GENERIC_COMMON(prefix)                                                   \
    using conn_type = typename base_type::conn_type;                                                        \
    using base_type::base_type;                                                                             \
    network_result<statement> prepare_statement(string_view stmt_sql) override                              \
    {                                                                                                       \
        return this->template fn_impl<statement, string_view>(                                              \
            &conn_type::prefix##prepare_statement,                                                          \
            stmt_sql                                                                                        \
        );                                                                                                  \
    }                                                                                                       \
    network_result<void> execute(string_view query, results& result) override                               \
    {                                                                                                       \
        return this->template fn_impl<void, const string_view&, results&>(                                  \
            &conn_type::prefix##execute,                                                                    \
            query,                                                                                          \
            result                                                                                          \
        );                                                                                                  \
    }                                                                                                       \
    network_result<void> execute(                                                                           \
        bound_statement_tuple<std::tuple<field_view, field_view>> req,                                      \
        results& result                                                                                     \
    ) override                                                                                              \
    {                                                                                                       \
        return this->template fn_impl<void, const typename base_type::stmt_tuple&, results&>(               \
            &conn_type::prefix##execute,                                                                    \
            req,                                                                                            \
            result                                                                                          \
        );                                                                                                  \
    }                                                                                                       \
    network_result<void> execute(bound_statement_iterator_range<fv_list_it> req, results& result) override  \
    {                                                                                                       \
        return this->template fn_impl<void, const typename base_type::stmt_it&, results&>(                  \
            &conn_type::prefix##execute,                                                                    \
            req,                                                                                            \
            result                                                                                          \
        );                                                                                                  \
    }                                                                                                       \
    network_result<void> start_execution(string_view query, execution_state& st) override                   \
    {                                                                                                       \
        return this->template fn_impl<void, const string_view&, execution_state&>(                          \
            &conn_type::prefix##start_execution,                                                            \
            query,                                                                                          \
            st                                                                                              \
        );                                                                                                  \
    }                                                                                                       \
    network_result<void> start_execution(                                                                   \
        bound_statement_tuple<std::tuple<field_view, field_view>> req,                                      \
        execution_state& st                                                                                 \
    ) override                                                                                              \
    {                                                                                                       \
        return this->template fn_impl<void, const typename base_type::stmt_tuple&, execution_state&>(       \
            &conn_type::prefix##start_execution,                                                            \
            req,                                                                                            \
            st                                                                                              \
        );                                                                                                  \
    }                                                                                                       \
    network_result<void> start_execution(                                                                   \
        bound_statement_iterator_range<fv_list_it> req,                                                     \
        execution_state& st                                                                                 \
    ) override                                                                                              \
    {                                                                                                       \
        return this->template fn_impl<void, const typename base_type::stmt_it&, execution_state&>(          \
            &conn_type::prefix##start_execution,                                                            \
            req,                                                                                            \
            st                                                                                              \
        );                                                                                                  \
    }                                                                                                       \
    network_result<void> close_statement(statement& stmt) override                                          \
    {                                                                                                       \
        return this->template fn_impl<void, const statement&>(&conn_type::prefix##close_statement, stmt);   \
    }                                                                                                       \
    network_result<void> read_resultset_head(execution_state& st) override                                  \
    {                                                                                                       \
        return this->template fn_impl<void, execution_state&>(&conn_type::prefix##read_resultset_head, st); \
    }                                                                                                       \
    network_result<rows_view> read_some_rows(execution_state& st) override                                  \
    {                                                                                                       \
        return this->template fn_impl<rows_view, execution_state&>(&conn_type::prefix##read_some_rows, st); \
    }                                                                                                       \
    network_result<void> ping() override { return this->template fn_impl<void>(&conn_type::prefix##ping); } \
    network_result<void> reset_connection() override                                                        \
    {                                                                                                       \
        return this->template fn_impl<void>(&conn_type::prefix##reset_connection);                          \
    }                                                                                                       \
    network_result<void> close() override                                                                   \
    {                                                                                                       \
        return this->template fn_impl<void>(&conn_type::prefix##close);                                     \
    }                                                                                                       \
    BOOST_MYSQL_TEST_IMPLEMENT_GENERIC_CXX14(prefix)

#define BOOST_MYSQL_TEST_IMPLEMENT_GENERIC(prefix)                                                           \
    network_result<void> connect(const handshake_params& params) override                                    \
    {                                                                                                        \
        return this->template fn_impl<                                                                       \
            void,                                                                                            \
            const typename Stream::lowest_layer_type::endpoint_type&,                                        \
            const handshake_params&>(&conn_type::prefix##connect, get_endpoint<Stream>(), params);           \
    }                                                                                                        \
    network_result<void> handshake(const handshake_params& params) override                                  \
    {                                                                                                        \
        return this->template fn_impl<void, const handshake_params&>(&conn_type::prefix##handshake, params); \
    }                                                                                                        \
    network_result<void> quit() override { return this->template fn_impl<void>(&conn_type::prefix##quit); }  \
    BOOST_MYSQL_TEST_IMPLEMENT_GENERIC_COMMON(prefix)

#define BOOST_MYSQL_TEST_IMPLEMENT_GENERIC_ANY(prefix)                    \
    network_result<void> connect(const handshake_params& params) override \
    {                                                                     \
        return this->template fn_impl<void, const connect_params&>(       \
            &conn_type::prefix##connect,                                  \
            get_connect_params(params)                                    \
        );                                                                \
    }                                                                     \
    BOOST_MYSQL_TEST_IMPLEMENT_GENERIC_COMMON(prefix)

// Use these
#define BOOST_MYSQL_TEST_IMPLEMENT_SYNC() BOOST_MYSQL_TEST_IMPLEMENT_GENERIC()
#define BOOST_MYSQL_TEST_IMPLEMENT_ASYNC() BOOST_MYSQL_TEST_IMPLEMENT_GENERIC(async_)
#define BOOST_MYSQL_TEST_IMPLEMENT_SYNC_ANY() BOOST_MYSQL_TEST_IMPLEMENT_GENERIC_ANY()
#define BOOST_MYSQL_TEST_IMPLEMENT_ASYNC_ANY() BOOST_MYSQL_TEST_IMPLEMENT_GENERIC_ANY(async_)

// Implementation for er_network_variant
template <class ErConnection>
class er_network_variant_impl : public er_network_variant
{
    using stream_type = typename ErConnection::stream_type;

public:
    bool supports_ssl() const override { return ::boost::mysql::test::supports_ssl<stream_type>(); }
    bool is_unix_socket() const override { return ::boost::mysql::test::is_unix_socket<stream_type>(); }
    bool supports_handshake() const override { return true; }
    const char* stream_name() const override { return ::boost::mysql::test::get_stream_name<stream_type>(); }
    const char* variant_name() const override { return ErConnection::name(); }
    er_connection_ptr create_connection(boost::asio::any_io_executor ex, boost::asio::ssl::context& ssl_ctx)
        override
    {
        return er_connection_ptr(new ErConnection(std::move(ex), ssl_ctx, *this));
    }
};

template <address_type addr_type>
constexpr const char* stream_name_from_type() noexcept;

template <>
constexpr const char* stream_name_from_type<address_type::host_and_port>() noexcept
{
    return "any_tcp";
}

template <>
constexpr const char* stream_name_from_type<address_type::unix_path>() noexcept
{
    return "any_unix";
}

template <address_type addr_type, class ErConnection>
class er_network_variant_any_impl : public er_network_variant
{
public:
    bool supports_ssl() const override { return addr_type == address_type::host_and_port; }
    bool is_unix_socket() const override { return addr_type == address_type::unix_path; }
    bool supports_handshake() const override { return false; }
    const char* stream_name() const override { return stream_name_from_type<addr_type>(); }
    const char* variant_name() const override { return ErConnection::name(); }
    er_connection_ptr create_connection(boost::asio::any_io_executor ex, boost::asio::ssl::context& ssl_ctx)
        override
    {
        return er_connection_ptr(new ErConnection(std::move(ex), ssl_ctx, *this, addr_type));
    }
};

// Helpers
inline void rethrow_on_failure(std::exception_ptr ptr)
{
    if (ptr)
    {
        std::rethrow_exception(ptr);
    }
}

template <class ErConnection>
void add_variant(std::vector<er_network_variant*>& output)
{
    static er_network_variant_impl<ErConnection> obj;
    output.push_back(&obj);
}

template <address_type addr_type, class ErConnection>
void add_variant_any(std::vector<er_network_variant*>& output)
{
    static er_network_variant_any_impl<addr_type, ErConnection> obj;
    output.push_back(&obj);
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
