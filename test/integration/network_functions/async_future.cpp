//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "network_functions_impl.hpp"
#include <boost/asio/use_future.hpp>

using namespace boost::mysql::test;
using boost::mysql::connection_params;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::value;
using boost::mysql::row;
using boost::mysql::execute_params;
using boost::asio::use_future;

namespace
{

// Helpers
template <class Callable>
auto impl_errinfo(
    Callable&& cb
) -> network_result<decltype(cb(std::declval<error_info&>()).get())>
{
    using R = decltype(cb(std::declval<error_info&>()).get()); // Callable returns a future<R>
    error_info info ("Error info was not clearer properly");
    std::future<R> fut = cb(info);
    try
    {
        R res = fut.get();
        return network_result<R>(
            error_code(),
            std::move(info),
            std::move(res)
        );
    }
    catch (const boost::system::system_error& err)
    {
        return network_result<R>(err.code(), std::move(info));
    }
}

template <class Callable>
network_result<no_result> impl_no_result_errinfo(
    Callable&& cb
)
{
    error_info info ("Error info was not clearer properly");
    std::future<void> fut = cb(info);
    try
    {
        fut.get();
        return network_result<no_result>(error_code(), std::move(info));
    }
    catch (const boost::system::system_error& err)
    {
        return network_result<no_result>(err.code(), std::move(info));
    }
}

template <class Callable>
auto impl_noerrinfo(
    Callable&& cb
) -> network_result<decltype(cb().get())>
{
    using R = decltype(cb().get()); // Callable returns a future<R>
    std::future<R> fut = cb();
    try
    {
        return network_result<R>(
            error_code(),
            fut.get()
        );
    }
    catch (const boost::system::system_error& err)
    {
        return network_result<R>(err.code());
    }
}

template <class Callable>
network_result<no_result> impl_no_result_noerrinfo(
    Callable&& cb
)
{
    std::future<void> fut = cb();
    try
    {
        fut.get();
        return network_result<no_result>(error_code());
    }
    catch (const boost::system::system_error& err)
    {
        return network_result<no_result>(err.code());
    }
}

// Base templates (no default completion tokens)
template <class Stream>
class async_future_errinfo : public network_functions<Stream>
{
public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    const char* name() const override { return "async_future_errinfo"; }
    network_result<no_result> connect(
        connection_type& conn,
        const typename Stream::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl_no_result_errinfo([&] (error_info& output_info) {
            return conn.async_connect(ep, params, output_info, use_future);
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl_no_result_errinfo([&] (error_info& output_info) {
            return conn.async_handshake(params, output_info, use_future);
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        boost::string_view query
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return conn.async_query(query, output_info, use_future);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        boost::string_view statement
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return conn.async_prepare_statement(statement, output_info, use_future);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const execute_params<value_list_it>& params
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return stmt.async_execute(params, output_info, use_future);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return stmt.async_execute(values, output_info, use_future);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl_no_result_errinfo([&] (error_info& output_info) {
            return stmt.async_close(output_info, use_future);
        });
    }
    network_result<bool> read_one(
        resultset_type& r,
		row& output
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return r.async_read_one(output, output_info, use_future);
        });
    }
    network_result<std::vector<row>> read_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return r.async_read_many(count, output_info, use_future);
        });
    }
    network_result<std::vector<row>> read_all(
        resultset_type& r
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return r.async_read_all(output_info, use_future);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl_no_result_errinfo([&] (error_info& output_info) {
            return conn.async_quit(output_info, use_future);
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl_no_result_errinfo([&] (error_info& output_info) {
            return conn.async_close(output_info, use_future);
        });
    }
};

template <class Stream>
class async_future_noerrinfo : public network_functions<Stream>
{
public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    const char* name() const override { return "async_future_noerrinfo"; }
    network_result<no_result> connect(
        connection_type& conn,
        const typename Stream::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl_no_result_noerrinfo([&] {
            return conn.async_connect(ep, params, use_future);
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl_no_result_noerrinfo([&] {
            return conn.async_handshake(params, use_future);
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        boost::string_view query
    ) override
    {
        return impl_noerrinfo([&] {
            return conn.async_query(query, use_future);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        boost::string_view statement
    ) override
    {
        return impl_noerrinfo([&]{
            return conn.async_prepare_statement(statement, use_future);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const execute_params<value_list_it>& params
    ) override
    {
        return impl_noerrinfo([&]{
            return stmt.async_execute(params, use_future);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl_noerrinfo([&] {
            return stmt.async_execute(values, use_future);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl_no_result_noerrinfo([&] {
            return stmt.async_close(use_future);
        });
    }
    network_result<bool> read_one(
        resultset_type& r,
		row& output
    ) override
    {
        return impl_noerrinfo([&] {
            return r.async_read_one(output, use_future);
        });
    }
    network_result<std::vector<row>> read_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl_noerrinfo([&] {
            return r.async_read_many(count, use_future);
        });
    }
    network_result<std::vector<row>> read_all(
        resultset_type& r
    ) override
    {
        return impl_noerrinfo([&] {
            return r.async_read_all(use_future);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl_no_result_noerrinfo([&] {
            return conn.async_quit(use_future);
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl_no_result_noerrinfo([&] {
            return conn.async_close(use_future);
        });
    }
};

// Default completion tokens
template <>
class async_future_errinfo<tcp_future_socket> : public network_functions<tcp_future_socket>
{
public:
    using connection_type = typename network_functions<tcp_future_socket>::connection_type;
    using prepared_statement_type = typename network_functions<tcp_future_socket>::prepared_statement_type;
    using resultset_type = typename network_functions<tcp_future_socket>::resultset_type;

    const char* name() const override { return "async_future_errinfo"; }
    network_result<no_result> connect(
        connection_type& conn,
        const typename tcp_future_socket::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl_no_result_errinfo([&] (error_info& output_info) {
            return conn.async_connect(ep, params, output_info);
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl_no_result_errinfo([&] (error_info& output_info) {
            return conn.async_handshake(params, output_info);
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        boost::string_view query
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return conn.async_query(query, output_info);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        boost::string_view statement
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return conn.async_prepare_statement(statement, output_info);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const execute_params<value_list_it>& params
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return stmt.async_execute(params, output_info);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return stmt.async_execute(values, output_info);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl_no_result_errinfo([&] (error_info& output_info) {
            return stmt.async_close(output_info);
        });
    }
    network_result<bool> read_one(
        resultset_type& r,
		row& output
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return r.async_read_one(output, output_info);
        });
    }
    network_result<std::vector<row>> read_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return r.async_read_many(count, output_info);
        });
    }
    network_result<std::vector<row>> read_all(
        resultset_type& r
    ) override
    {
        return impl_errinfo([&] (error_info& output_info) {
            return r.async_read_all(output_info);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl_no_result_errinfo([&] (error_info& output_info) {
            return conn.async_quit(output_info);
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl_no_result_errinfo([&] (error_info& output_info) {
            return conn.async_close(output_info);
        });
    }
};

template <>
class async_future_noerrinfo<tcp_future_socket> : public network_functions<tcp_future_socket>
{
public:
    using connection_type = typename network_functions<tcp_future_socket>::connection_type;
    using prepared_statement_type = typename network_functions<tcp_future_socket>::prepared_statement_type;
    using resultset_type = typename network_functions<tcp_future_socket>::resultset_type;

    const char* name() const override { return "async_future_noerrinfo"; }
    network_result<no_result> connect(
        connection_type& conn,
        const typename tcp_future_socket::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl_no_result_noerrinfo([&] {
            return conn.async_connect(ep, params);
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl_no_result_noerrinfo([&] {
            return conn.async_handshake(params);
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        boost::string_view query
    ) override
    {
        return impl_noerrinfo([&] {
            return conn.async_query(query);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        boost::string_view statement
    ) override
    {
        return impl_noerrinfo([&]{
            return conn.async_prepare_statement(statement);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const execute_params<value_list_it>& params
    ) override
    {
        return impl_noerrinfo([&]{
            return stmt.async_execute(params);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl_noerrinfo([&] {
            return stmt.async_execute(values);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl_no_result_noerrinfo([&] {
            return stmt.async_close();
        });
    }
    network_result<bool> read_one(
        resultset_type& r,
		row& output
    ) override
    {
        return impl_noerrinfo([&] {
            return r.async_read_one(output);
        });
    }
    network_result<std::vector<row>> read_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl_noerrinfo([&] {
            return r.async_read_many(count);
        });
    }
    network_result<std::vector<row>> read_all(
        resultset_type& r
    ) override
    {
        return impl_noerrinfo([&] {
            return r.async_read_all();
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl_no_result_noerrinfo([&] {
            return conn.async_quit();
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl_no_result_noerrinfo([&] {
            return conn.async_close();
        });
    }
};

} // anon namespace

// Visible stuff
template <class Stream>
network_functions<Stream>* boost::mysql::test::async_future_errinfo_functions()
{
    static async_future_errinfo<Stream> res;
    return &res;
}

template <class Stream>
network_functions<Stream>* boost::mysql::test::async_future_noerrinfo_functions()
{
    static async_future_noerrinfo<Stream> res;
    return &res;
}

BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_future_errinfo_functions)
BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_future_noerrinfo_functions)

template network_functions<tcp_future_socket>*
boost::mysql::test::async_future_errinfo_functions<tcp_future_socket>();

template network_functions<tcp_future_socket>*
boost::mysql::test::async_future_noerrinfo_functions<tcp_future_socket>();

