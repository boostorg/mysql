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
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement_base.hpp>

#include <boost/asio/spawn.hpp>

#include <memory>

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

namespace {

template <class Callable>
using impl_result_type = decltype(std::declval<Callable>(
)(std::declval<boost::asio::yield_context>(), std::declval<error_info&>()));

template <class IoObj, class Callable>
network_result<impl_result_type<Callable>> impl(IoObj& obj, Callable&& cb)
{
    using R = impl_result_type<Callable>;

    std::promise<network_result<R>> prom;

    boost::asio::spawn(obj.get_executor(), [&](boost::asio::yield_context yield) {
        error_code ec = boost::mysql::make_error_code(boost::mysql::errc::no);
        error_info info("error_info not cleared properly");
        R result = cb(yield[ec], info);
        prom.set_value(network_result<R>(ec, std::move(info), std::move(result)));
    });

    return wait_for_result(prom);
}

template <class Stream>
class async_coroutine_resultset : public er_resultset_base<Stream>
{
public:
    network_result<row_view> read_one() override
    {
        return impl(this->obj(), [&](boost::asio::yield_context yield, error_info& info) {
            return this->obj().async_read_one(use_views, info, yield);
        });
    }
    network_result<rows_view> read_some() override
    {
        return impl(this->obj(), [&](boost::asio::yield_context yield, error_info& info) {
            return this->obj().async_read_some(use_views, info, yield);
        });
    }
    network_result<rows_view> read_all() override
    {
        return impl(this->obj(), [&](boost::asio::yield_context yield, error_info& info) {
            return this->obj().async_read_all(use_views, info, yield);
        });
    }
};

template <class Stream>
class async_coroutine_statement : public er_statement_base<Stream>
{
public:
    network_result<no_result> execute_tuple_1(
        field_view param,
        const execute_options& opts,
        er_resultset& result
    ) override
    {
        return impl(this->obj(), [&](boost::asio::yield_context yield, error_info& info) {
            this->obj()
                .async_execute(std::make_tuple(param), opts, this->cast(result), info, yield);
            return no_result();
        });
    }
    network_result<no_result> execute_tuple_2(
        field_view param1,
        field_view param2,
        er_resultset& result
    ) override
    {
        return impl(this->obj(), [&](boost::asio::yield_context yield, error_info& info) {
            this->obj()
                .async_execute(std::make_tuple(param1, param2), this->cast(result), info, yield);
            return no_result();
        });
    }
    network_result<no_result> execute_it(
        value_list_it params_first,
        value_list_it params_last,
        const execute_options& opts,
        er_resultset& result
    ) override
    {
        return impl(this->obj(), [&](boost::asio::yield_context yield, error_info& info) {
            this->obj()
                .async_execute(params_first, params_last, opts, this->cast(result), info, yield);
            return no_result();
        });
    }
    network_result<no_result> close() override
    {
        return impl(this->obj(), [&](boost::asio::yield_context yield, error_info& info) {
            this->obj().async_close(info, yield);
            return no_result();
        });
    }
};

template <class Stream>
class async_coroutine_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;

    network_result<no_result> physical_connect(er_endpoint kind) override
    {
        return impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            info.clear();
            this->conn_.stream().lowest_layer().async_connect(get_endpoint<Stream>(kind), yield);
            return no_result();
        });
    }
    network_result<no_result> connect(er_endpoint kind, const handshake_params& params) override
    {
        return impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            this->conn_.async_connect(get_endpoint<Stream>(kind), params, info, yield);
            return no_result();
        });
    }
    network_result<no_result> handshake(const handshake_params& params) override
    {
        return impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            this->conn_.async_handshake(params, info, yield);
            return no_result();
        });
    }
    network_result<no_result> query(boost::string_view query, er_resultset& result) override
    {
        return impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            this->conn_.async_query(query, this->cast(result), info, yield);
            return no_result();
        });
    }
    network_result<no_result> prepare_statement(boost::string_view statement, er_statement& stmt)
        override
    {
        return impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            this->conn_.async_prepare_statement(statement, this->cast(stmt), info, yield);
            return no_result();
        });
    }
    network_result<no_result> quit() override
    {
        return impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            this->conn_.async_quit(info, yield);
            return no_result();
        });
    }
    network_result<no_result> close() override
    {
        return impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            this->conn_.async_close(info, yield);
            return no_result();
        });
    }
};

template <class Stream>
class async_coroutine_variant : public er_network_variant_base<
                                    Stream,
                                    async_coroutine_connection,
                                    async_coroutine_statement,
                                    async_coroutine_resultset>
{
public:
    const char* variant_name() const override { return "async_coroutine"; }
};

}  // namespace

void boost::mysql::test::add_async_coroutine(std::vector<er_network_variant*>& output)
{
    static async_coroutine_variant<tcp_socket> tcp;
    output.push_back(&tcp);
}
