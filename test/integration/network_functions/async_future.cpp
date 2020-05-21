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
using boost::mysql::errc;
using boost::mysql::value;
using boost::mysql::row;
using boost::mysql::owning_row;
using boost::asio::use_future;

namespace
{

template <typename Stream>
class async_future : public network_functions<Stream>
{
    template <typename Callable>
    static auto impl(Callable&& cb) {
        using R = decltype(cb().get()); // Callable returns a future<R>
        std::future<R> fut = cb();
        try
        {
            // error_info is not available here, so we skip validation
            return network_result<R>(
                error_code(),
                fut.get()
            );
        }
        catch (const boost::system::system_error& err)
        {
            // error_info is not available here, so we skip validation
            return network_result<R>(err.code());
        }
    }

    template <typename Callable>
    static network_result<no_result> impl_no_result(Callable&& cb) {
        std::future<void> fut = cb();
        try
        {
            // error_info is not available here, so we skip validation
            fut.get();
            return network_result<no_result>(error_code());
        }
        catch (const boost::system::system_error& err)
        {
            // error_info is not available here, so we skip validation
            return network_result<no_result>(err.code());
        }
    }
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
        return impl_no_result([&] {
            return conn.async_connect(ep, params, use_future);
        });
    }
    network_result<no_result> handshake(
        connection_type& conn,
        const connection_params& params
    ) override
    {
        return impl_no_result([&] {
            return conn.async_handshake(params, use_future);
        });
    }
    network_result<resultset_type> query(
        connection_type& conn,
        std::string_view query
    ) override
    {
        return impl([&] {
            return conn.async_query(query, use_future);
        });
    }
    network_result<prepared_statement_type> prepare_statement(
        connection_type& conn,
        std::string_view statement
    ) override
    {
        return impl([&]{
            return conn.async_prepare_statement(statement, use_future);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        value_list_it params_first,
        value_list_it params_last
    ) override
    {
        return impl([&]{
            return stmt.async_execute(params_first, params_last, use_future);
        });
    }
    network_result<resultset_type> execute_statement(
        prepared_statement_type& stmt,
        const std::vector<value>& values
    ) override
    {
        return impl([&] {
            return stmt.async_execute(values, use_future);
        });
    }
    network_result<no_result> close_statement(
        prepared_statement_type& stmt
    ) override
    {
        return impl_no_result([&] {
            return stmt.async_close(use_future);
        });
    }
    network_result<const row*> fetch_one(
        resultset_type& r
    ) override
    {
        return impl([&] {
            return r.async_fetch_one(use_future);
        });
    }
    network_result<std::vector<owning_row>> fetch_many(
        resultset_type& r,
        std::size_t count
    ) override
    {
        return impl([&] {
            return r.async_fetch_many(count, use_future);
        });
    }
    network_result<std::vector<owning_row>> fetch_all(
        resultset_type& r
    ) override
    {
        return impl([&] {
            return r.async_fetch_all(use_future);
        });
    }
    network_result<no_result> quit(
        connection_type& conn
    ) override
    {
        return impl_no_result([&] {
            return conn.async_quit(use_future);
        });
    }
    network_result<no_result> close(
        connection_type& conn
    ) override
    {
        return impl_no_result([&] {
            return conn.async_close(use_future);
        });
    }
};

} // anon namespace

// Visible stuff
template <typename Stream>
network_functions<Stream>* boost::mysql::test::async_future_functions()
{
    static async_future<Stream> res;
    return &res;
}

BOOST_MYSQL_INSTANTIATE_NETWORK_FUNCTIONS(async_future_functions)
