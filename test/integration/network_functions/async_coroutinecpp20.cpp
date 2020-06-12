//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "network_functions_impl.hpp"
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <future>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

using namespace boost::mysql::test;
using boost::mysql::connection_params;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::value;
using boost::mysql::row;
using boost::mysql::owning_row;
using boost::asio::use_awaitable;

namespace
{

template <typename Stream>
class async_coroutinecpp20_errinfo : public network_functions<Stream>
{
    template <typename Callable>
    using impl_result_type = typename decltype(std::declval<Callable>()(
        std::declval<error_info&>()
    ))::value_type;

    template <typename IoObj, typename Callable>
    network_result<impl_result_type<Callable>> impl(IoObj& obj, Callable&& cb) {
        using R = impl_result_type<Callable>;
        std::promise<network_result<R>> prom;

        boost::asio::co_spawn(obj.get_executor(), [&] () -> boost::asio::awaitable<void> {
            error_info info ("error_info not cleared properly");
            try
            {
                R result = co_await cb(info);
                prom.set_value(network_result<R>(error_code(), std::move(info), std::move(result)));
            }
            catch (const boost::system::system_error& err)
            {
                prom.set_value(network_result<R>(err.code(), std::move(info)));
            }
        }, boost::asio::detached);

        return prom.get_future().get();
    }

    template <typename IoObj, typename Callable>
    network_result<no_result> impl_no_result(IoObj& obj, Callable&& cb) {
        std::promise<network_result<no_result>> prom;

        boost::asio::co_spawn(obj.get_executor(), [&] () -> boost::asio::awaitable<void> {
            error_info info ("error_info not cleared properly");
            try
            {
                co_await cb(info);
                prom.set_value(network_result<no_result>(error_code(), std::move(info)));
            }
            catch (const boost::system::system_error& err)
            {
                prom.set_value(network_result<no_result>(err.code(), std::move(info)));
            }
        }, boost::asio::detached);

        return prom.get_future().get();
    }

public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    const char* name() const override
    {
        return "async_coroutinecpp20_errinfo";
    }
    network_result<no_result> connect(
        connection_type& conn,
        const typename Stream::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl_no_result(conn, [&](error_info& info) {
            return conn.async_connect(ep, params, info, use_awaitable);
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl_no_result(conn, [&](error_info& info) {
            return conn.async_handshake(params, info, use_awaitable);
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        boost::string_view query
    ) override
    {
        return impl(conn, [&](error_info& info) {
            return conn.async_query(query, info, use_awaitable);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        boost::string_view statement
    ) override
    {
        return impl(conn, [&](error_info& info) {
            return conn.async_prepare_statement(statement, info, use_awaitable);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        value_list_it params_first,
        value_list_it params_last
    ) override
    {
        return impl(stmt, [&](error_info& info) {
            return stmt.async_execute(params_first, params_last, info, use_awaitable);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl(stmt, [&](error_info& info) {
            return stmt.async_execute(values, info, use_awaitable);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl_no_result(stmt, [&](error_info& info) {
            return stmt.async_close(info, use_awaitable);
        });
    }
    network_result<const row*> fetch_one(
        resultset_type& r
    ) override
    {
        return impl(r, [&](error_info& info) {
            return r.async_fetch_one(info, use_awaitable);
        });
    }
    network_result<std::vector<owning_row>> fetch_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl(r, [&](error_info& info) {
            return r.async_fetch_many(count, info, use_awaitable);
        });
    }
    network_result<std::vector<owning_row>> fetch_all(
        resultset_type& r
    ) override
    {
        return impl(r, [&](error_info& info) {
            return r.async_fetch_all(info, use_awaitable);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl_no_result(conn, [&](error_info& info) {
            return conn.async_quit(info, use_awaitable);
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl_no_result(conn, [&](error_info& info) {
            return conn.async_close(info, use_awaitable);
        });
    }
};

template <typename Stream>
class async_coroutinecpp20_noerrinfo : public network_functions<Stream>
{
    template <typename Callable>
    using impl_result_type = typename decltype(std::declval<Callable>()())::value_type;

    template <typename IoObj, typename Callable>
    network_result<impl_result_type<Callable>> impl(IoObj& obj, Callable&& cb)
    {
        using R = impl_result_type<Callable>;
        std::promise<network_result<R>> prom;

        boost::asio::co_spawn(obj.get_executor(), [&] () -> boost::asio::awaitable<void> {
            try
            {
                prom.set_value(network_result<R>(error_code(), co_await cb()));
            }
            catch (const boost::system::system_error& err)
            {
                prom.set_value(network_result<R>(err.code()));
            }
        }, boost::asio::detached);

        return prom.get_future().get();
    }

    template <typename IoObj, typename Callable>
    network_result<no_result> impl_no_result(IoObj& obj, Callable&& cb)
    {
        std::promise<network_result<no_result>> prom;

        boost::asio::co_spawn(obj.get_executor(), [&] () -> boost::asio::awaitable<void> {
            try
            {
                co_await cb();
                prom.set_value(network_result<no_result>(error_code()));
            }
            catch (const boost::system::system_error& err)
            {
                prom.set_value(network_result<no_result>(err.code()));
            }
        }, boost::asio::detached);

        return prom.get_future().get();
    }

public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    const char* name() const override
    {
        return "async_coroutinecpp20_noerrinfo";
    }
    network_result<no_result> connect(
        connection_type& conn,
        const typename Stream::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl_no_result(conn, [&] {
            return conn.async_connect(ep, params, use_awaitable);
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl_no_result(conn, [&] {
            return conn.async_handshake(params, use_awaitable);
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        boost::string_view query
    ) override
    {
        return impl(conn, [&] {
            return conn.async_query(query, use_awaitable);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        boost::string_view statement
    ) override
    {
        return impl(conn, [&] {
            return conn.async_prepare_statement(statement, use_awaitable);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        value_list_it params_first,
        value_list_it params_last
    ) override
    {
        return impl(stmt, [&] {
            return stmt.async_execute(params_first, params_last, use_awaitable);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl(stmt, [&] {
            return stmt.async_execute(values, use_awaitable);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl_no_result(stmt, [&] {
            return stmt.async_close(use_awaitable);
        });
    }
    network_result<const row*> fetch_one(
        resultset_type& r
    ) override
    {
        return impl(r, [&] {
            return r.async_fetch_one(use_awaitable);
        });
    }
    network_result<std::vector<owning_row>> fetch_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl(r, [&] {
            return r.async_fetch_many(count, use_awaitable);
        });
    }
    network_result<std::vector<owning_row>> fetch_all(
        resultset_type& r
    ) override
    {
        return impl(r, [&] {
            return r.async_fetch_all(use_awaitable);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl_no_result(conn, [&] {
            return conn.async_quit(use_awaitable);
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl_no_result(conn, [&] {
            return conn.async_close(use_awaitable);
        });
    }
};

} // anon namespace

// Visible stuff
// Visible stuff
template <typename Stream>
network_functions<Stream>* boost::mysql::test::async_coroutinecpp20_errinfo_functions()
{
    static async_coroutinecpp20_errinfo<Stream> res;
    return &res;
}

template <typename Stream>
network_functions<Stream>* boost::mysql::test::async_coroutinecpp20_noerrinfo_functions()
{
    static async_coroutinecpp20_noerrinfo<Stream> res;
    return &res;
}

BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_coroutinecpp20_errinfo_functions)
BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_coroutinecpp20_noerrinfo_functions)

#endif // BOOST_ASIO_HAS_CO_AWAIT
