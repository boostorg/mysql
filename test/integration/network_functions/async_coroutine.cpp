//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "network_functions_impl.hpp"
#include <boost/asio/spawn.hpp>
#include <future>

using namespace boost::mysql::test;
using boost::mysql::connection_params;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::errc;
using boost::mysql::value;
using boost::mysql::row;
using boost::asio::yield_context;
using boost::mysql::execute_params;

namespace
{

template <class Stream>
class async_coroutine_errinfo : public network_functions<Stream>
{
    template <class Callable>
    using impl_result_type = decltype(std::declval<Callable>()(
        std::declval<yield_context>(),
        std::declval<error_info&>()
    ));

    template <class IoObj, class Callable>
    network_result<impl_result_type<Callable>> impl(IoObj& obj, Callable&& cb)
    {
        using R = impl_result_type<Callable>;

        std::promise<network_result<R>> prom;

        boost::asio::spawn(obj.get_executor(), [&](yield_context yield) {
            error_code ec = boost::mysql::detail::make_error_code(errc::no);
            error_info info ("error_info not cleared properly");
            R result = cb(yield[ec], info);
            prom.set_value(network_result<R>(ec, std::move(info), std::move(result)));
        });

        return prom.get_future().get();
    }

public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    const char* name() const override
    {
        return "async_coroutine_errinfo";
    }
    network_result<no_result> connect(
        connection_type& conn,
        const typename Stream::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl(conn, [&](yield_context yield, error_info& info) {
            conn.async_connect(ep, params, info, yield);
            return no_result();
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl(conn, [&](yield_context yield, error_info& info) {
            conn.async_handshake(params, info, yield);
            return no_result();
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        boost::string_view query
    ) override
    {
        return impl(conn, [&](yield_context yield, error_info& info) {
            return conn.async_query(query, info, yield);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        boost::string_view statement
    ) override
    {
        return impl(conn, [&](yield_context yield, error_info& info) {
            return conn.async_prepare_statement(statement, info, yield);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const execute_params<value_list_it>& params
    ) override
    {
        return impl(stmt, [&](yield_context yield, error_info& info) {
            return stmt.async_execute(params, info, yield);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl(stmt, [&](yield_context yield, error_info& info) {
            return stmt.async_execute(values, info, yield);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl(stmt, [&](yield_context yield, error_info& info) {
            stmt.async_close(info, yield);
            return no_result();
        });
    }
    network_result<bool> read_one(
        resultset_type& r,
		row& output
    ) override
    {
        return impl(r, [&](yield_context yield, error_info& info) {
            return r.async_read_one(output, info, yield);
        });
    }
    network_result<std::vector<row>> read_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl(r, [&](yield_context yield, error_info& info) {
            return r.async_read_many(count, info, yield);
        });
    }
    network_result<std::vector<row>> read_all(
        resultset_type& r
    ) override
    {
        return impl(r, [&](yield_context yield, error_info& info) {
            return r.async_read_all(info, yield);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl(conn, [&](yield_context yield, error_info& info) {
            conn.async_quit(info, yield);
            return no_result();
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl(conn, [&](yield_context yield, error_info& info) {
            conn.async_close(info, yield);
            return no_result();
        });
    }
};

template <class Stream>
class async_coroutine_noerrinfo : public network_functions<Stream>
{
    template <class Callable>
    using impl_result_type = decltype(std::declval<Callable>()(
        std::declval<yield_context>()
    ));

    template <class IoObj, class Callable>
    network_result<impl_result_type<Callable>> impl(IoObj& obj, Callable&& cb)
    {
        using R = impl_result_type<Callable>;

        std::promise<network_result<R>> prom;

        boost::asio::spawn(obj.get_executor(), [&](yield_context yield) {
            error_code ec = boost::mysql::detail::make_error_code(errc::no);
            R result = cb(yield[ec]);
            prom.set_value(network_result<R>(ec, std::move(result)));
        });

        return prom.get_future().get();
    }

public:
    using connection_type = typename network_functions<Stream>::connection_type;
    using prepared_statement_type = typename network_functions<Stream>::prepared_statement_type;
    using resultset_type = typename network_functions<Stream>::resultset_type;

    const char* name() const override
    {
        return "async_coroutine_noerrinfo";
    }
    network_result<no_result> connect(
        connection_type& conn,
        const typename Stream::endpoint_type& ep,
        const connection_params& params
    ) override
    {
        return impl(conn, [&](yield_context yield) {
            conn.async_connect(ep, params, yield);
            return no_result();
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl(conn, [&](yield_context yield) {
            conn.async_handshake(params, yield);
            return no_result();
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        boost::string_view query
    ) override
    {
        return impl(conn, [&](yield_context yield) {
            return conn.async_query(query, yield);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        boost::string_view statement
    ) override
    {
        return impl(conn, [&](yield_context yield) {
            return conn.async_prepare_statement(statement, yield);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const execute_params<value_list_it>& params
    ) override
    {
        return impl(stmt, [&](yield_context yield) {
            return stmt.async_execute(params, yield);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl(stmt, [&](yield_context yield) {
            return stmt.async_execute(values, yield);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl(stmt, [&](yield_context yield) {
            stmt.async_close(yield);
            return no_result();
        });
    }
    network_result<bool> read_one(
        resultset_type& r,
		row& output
    ) override
    {
        return impl(r, [&](yield_context yield) {
            return r.async_read_one(output, yield);
        });
    }
    network_result<std::vector<row>> read_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl(r, [&](yield_context yield) {
            return r.async_read_many(count, yield);
        });
    }
    network_result<std::vector<row>> read_all(
        resultset_type& r
    ) override
    {
        return impl(r, [&](yield_context yield) {
            return r.async_read_all(yield);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl(conn, [&](yield_context yield) {
            conn.async_quit(yield);
            return no_result();
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl(conn, [&](yield_context yield) {
            conn.async_close(yield);
            return no_result();
        });
    }
};

} // anon namespace

// Visible stuff
template <class Stream>
network_functions<Stream>* boost::mysql::test::async_coroutine_errinfo_functions()
{
    static async_coroutine_errinfo<Stream> res;
    return &res;
}

template <class Stream>
network_functions<Stream>* boost::mysql::test::async_coroutine_noerrinfo_functions()
{
    static async_coroutine_noerrinfo<Stream> res;
    return &res;
}

BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_coroutine_errinfo_functions)
BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_coroutine_noerrinfo_functions)
