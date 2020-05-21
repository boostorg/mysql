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
class async_coroutinecpp20 : public network_functions<Stream>
{
    bool use_errinfo_;

    template <typename IoObj, typename Callable>
    auto impl(IoObj& obj, Callable&& cb) {
        using R = typename decltype(cb(std::declval<error_info*>()))::value_type;
        std::promise<network_result<R>> prom;

        boost::asio::co_spawn(obj.get_executor(), [&, this] () -> boost::asio::awaitable<void> {
            error_info info ("error_info not cleared properly");
            try
            {
                R result = co_await cb(use_errinfo_ ? &info : nullptr);
                if (use_errinfo_)
                {
                    prom.set_value(network_result<R>(error_code(), std::move(info), std::move(result)));
                }
                else
                {
                    prom.set_value(network_result<R>(error_code(), std::move(result)));
                }
            }
            catch (const boost::system::system_error& err)
            {
                if (use_errinfo_)
                {
                    prom.set_value(network_result<R>(err.code(), std::move(info)));
                }
                else
                {
                    prom.set_value(network_result<R>(err.code()));
                }
            }
        }, boost::asio::detached);

        return prom.get_future().get();
    }

    template <typename IoObj, typename Callable>
    auto impl_no_result(IoObj& obj, Callable&& cb) {
        std::promise<network_result<no_result>> prom;

        boost::asio::co_spawn(obj.get_executor(), [&, this] () -> boost::asio::awaitable<void> {
            error_info info ("error_info not cleared properly");
            try
            {
                co_await cb(use_errinfo_ ? &info : nullptr);
                if (use_errinfo_)
                {
                    prom.set_value(network_result<no_result>(error_code(), std::move(info)));
                }
                else
                {
                    prom.set_value(network_result<no_result>(error_code()));
                }
            }
            catch (const boost::system::system_error& err)
            {
                if (use_errinfo_)
                {
                    prom.set_value(network_result<no_result>(err.code(), std::move(info)));
                }
                else
                {
                    prom.set_value(network_result<no_result>(err.code()));
                }
            }
        }, boost::asio::detached);

        return prom.get_future().get();
    }

public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    async_coroutinecpp20(bool use_errinfo): use_errinfo_(use_errinfo) {}
    const char* name() const override
    {
        return use_errinfo_ ? "async_coroutinecpp20_errinfo" : "async_coroutinecpp20_noerrinfo";
    }
    network_result<no_result> connect(
        connection_type& conn,
        const typename Stream::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl_no_result(conn, [&](error_info* info) {
            return conn.async_connect(ep, params, use_awaitable, info);
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl_no_result(conn, [&](error_info* info) {
            return conn.async_handshake(params, use_awaitable, info);
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        std::string_view query
    ) override
    {
        return impl(conn, [&](error_info* info) {
            return conn.async_query(query, use_awaitable, info);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        std::string_view statement
    ) override
    {
        return impl(conn, [&](error_info* info) {
            return conn.async_prepare_statement(statement, use_awaitable, info);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        value_list_it params_first,
        value_list_it params_last
    ) override
    {
        return impl(stmt, [&](error_info* info) {
            return stmt.async_execute(params_first, params_last, use_awaitable, info);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl(stmt, [&](error_info* info) {
            return stmt.async_execute(values, use_awaitable, info);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl_no_result(stmt, [&](error_info* info) {
            return stmt.async_close(use_awaitable, info);
        });
    }
    network_result<const row*> fetch_one(
        resultset_type& r
    ) override
    {
        return impl(r, [&](error_info* info) {
            return r.async_fetch_one(use_awaitable, info);
        });
    }
    network_result<std::vector<owning_row>> fetch_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl(r, [&](error_info* info) {
            return r.async_fetch_many(count, use_awaitable, info);
        });
    }
    network_result<std::vector<owning_row>> fetch_all(
        resultset_type& r
    ) override
    {
        return impl(r, [&](error_info* info) {
            return r.async_fetch_all(use_awaitable, info);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl_no_result(conn, [&](error_info* info) {
            return conn.async_quit(use_awaitable, info);
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl_no_result(conn, [&](error_info* info) {
            return conn.async_close(use_awaitable, info);
        });
    }
};

} // anon namespace

// Visible stuff
// Visible stuff
template <typename Stream>
network_functions<Stream>* boost::mysql::test::async_coroutinecpp20_errinfo_functions()
{
    static async_coroutinecpp20<Stream> res (true);
    return &res;
}

template <typename Stream>
network_functions<Stream>* boost::mysql::test::async_coroutinecpp20_noerrinfo_functions()
{
    static async_coroutinecpp20<Stream> res (false);
    return &res;
}

BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_coroutinecpp20_errinfo_functions)
BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_coroutinecpp20_noerrinfo_functions)

#endif // BOOST_ASIO_HAS_CO_AWAIT
