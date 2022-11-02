//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include <boost/mysql/errc.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/execute_options.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement_base.hpp>
#include <boost/mysql/use_views.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <memory>
#include <tuple>

#include "er_connection.hpp"
#include "er_impl_common.hpp"
#include "er_network_variant.hpp"
#include "er_resultset.hpp"
#include "er_statement.hpp"
#include "get_endpoint.hpp"
#include "handler_call_tracker.hpp"
#include "network_result.hpp"
#include "streams.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::execute_options;
using boost::mysql::field_view;
using boost::mysql::handshake_params;
using boost::mysql::row_view;
using boost::mysql::rows_view;
using boost::mysql::use_views;

#ifdef BOOST_ASIO_HAS_CO_AWAIT

namespace {

template <class Callable>
using impl_result_type = typename decltype(std::declval<Callable>()(std::declval<error_info&>())
)::value_type;

template <class IoObj, class Callable>
network_result<impl_result_type<Callable>> impl(IoObj& obj, Callable&& cb)
{
    using R = impl_result_type<Callable>;
    std::promise<network_result<R>> prom;

    boost::asio::co_spawn(
        obj.get_executor(),
        [&]() -> boost::asio::awaitable<void> {
            error_info info("error_info not cleared properly");
            try
            {
                R result = co_await cb(info);
                prom.set_value(network_result<R>(error_code(), std::move(info), std::move(result)));
            }
            catch (const boost::system::system_error& err)
            {
                prom.set_value(network_result<R>(err.code(), std::move(info)));
            }
        },
        boost::asio::detached
    );

    return wait_for_result(prom);
}

template <class IoObj, class Callable>
network_result<no_result> impl_no_result(IoObj& obj, Callable&& cb)
{
    std::promise<network_result<no_result>> prom;

    boost::asio::co_spawn(
        obj.get_executor(),
        [&]() -> boost::asio::awaitable<void> {
            error_info info("error_info not cleared properly");
            try
            {
                co_await cb(info);
                prom.set_value(network_result<no_result>(error_code(), std::move(info)));
            }
            catch (const boost::system::system_error& err)
            {
                prom.set_value(network_result<no_result>(err.code(), std::move(info)));
            }
        },
        boost::asio::detached
    );

    return wait_for_result(prom);
}

template <class Stream>
class async_coroutinecpp20_resultset : public er_resultset_base<Stream>
{
public:
    network_result<row_view> read_one() override
    {
        return impl(this->obj(), [&](error_info& info) {
            return this->obj().async_read_one(use_views, info, boost::asio::use_awaitable);
        });
    }
    network_result<rows_view> read_some() override
    {
        return impl(this->obj(), [&](error_info& info) {
            return this->obj().async_read_some(use_views, info, boost::asio::use_awaitable);
        });
    }
    network_result<rows_view> read_all() override
    {
        return impl(this->obj(), [&](error_info& info) {
            return this->obj().async_read_all(use_views, info, boost::asio::use_awaitable);
        });
    }
};

template <class Stream>
class async_coroutinecpp20_statement : public er_statement_base<Stream>
{
public:
    network_result<no_result> execute_tuple_1(
        field_view param,
        const execute_options& opts,
        er_resultset& result
    ) override
    {
        return impl_no_result(this->obj(), [&](error_info& info) {
            return this->obj().async_execute(
                std::make_tuple(param),
                opts,
                this->cast(result),
                info,
                boost::asio::use_awaitable
            );
        });
    }
    network_result<no_result> execute_tuple_2(
        field_view param1,
        field_view param2,
        er_resultset& result
    ) override
    {
        return impl_no_result(this->obj(), [&](error_info& info) {
            return this->obj().async_execute(
                std::make_tuple(param1, param2),
                this->cast(result),
                info,
                boost::asio::use_awaitable
            );
        });
    }
    network_result<no_result> execute_it(
        value_list_it params_first,
        value_list_it params_last,
        const execute_options& opts,
        er_resultset& result
    ) override
    {
        return impl_no_result(this->obj(), [&](error_info& info) {
            return this->obj().async_execute(
                params_first,
                params_last,
                opts,
                this->cast(result),
                info,
                boost::asio::use_awaitable
            );
        });
    }
    network_result<no_result> close() override
    {
        return impl_no_result(this->obj(), [&](error_info& info) {
            return this->obj().async_close(info, boost::asio::use_awaitable);
        });
    }
};

template <class Stream>
class async_coroutinecpp20_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;

    network_result<no_result> physical_connect(er_endpoint kind) override
    {
        return impl_no_result(this->conn_, [&](error_info& info) {
            info.clear();

            return this->conn_.stream().lowest_layer().async_connect(
                get_endpoint<Stream>(kind),
                boost::asio::use_awaitable
            );
        });
    }
    network_result<no_result> connect(er_endpoint kind, const handshake_params& params) override
    {
        return impl_no_result(this->conn_, [&](error_info& info) {
            return this->conn_.async_connect(
                get_endpoint<Stream>(kind),
                params,
                info,
                boost::asio::use_awaitable
            );
        });
    }
    network_result<no_result> handshake(const handshake_params& params) override
    {
        return impl_no_result(this->conn_, [&](error_info& info) {
            return this->conn_.async_handshake(params, info, boost::asio::use_awaitable);
        });
    }
    network_result<no_result> query(boost::string_view query, er_resultset& result) override
    {
        return impl_no_result(this->conn_, [&](error_info& info) {
            return this->conn_
                .async_query(query, this->cast(result), info, boost::asio::use_awaitable);
        });
    }
    network_result<no_result> prepare_statement(boost::string_view statement, er_statement& stmt)
        override
    {
        return impl_no_result(this->conn_, [&](error_info& info) {
            return this->conn_.async_prepare_statement(
                statement,
                this->cast(stmt),
                info,
                boost::asio::use_awaitable
            );
        });
    }
    network_result<no_result> quit() override
    {
        return impl_no_result(this->conn_, [&](error_info& info) {
            return this->conn_.async_quit(info, boost::asio::use_awaitable);
        });
    }
    network_result<no_result> close() override
    {
        return impl_no_result(this->conn_, [&](error_info& info) {
            return this->conn_.async_close(info, boost::asio::use_awaitable);
        });
    }
};

template <class Stream>
class async_coroutinecpp20_variant : public er_network_variant_base<
                                         Stream,
                                         async_coroutinecpp20_connection,
                                         async_coroutinecpp20_statement,
                                         async_coroutinecpp20_resultset>
{
public:
    const char* variant_name() const override { return "async_coroutinecpp20"; }
};

}  // namespace

void boost::mysql::test::add_async_coroutinecpp20(std::vector<er_network_variant*>& output)
{
    static async_coroutinecpp20_variant<tcp_socket> tcp;
    output.push_back(&tcp);
}

#else

void boost::mysql::test::add_async_coroutinecpp20(std::vector<er_network_variant*>&) {}

#endif
