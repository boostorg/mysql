//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/server_diagnostics.hpp>
#include <boost/mysql/statement_base.hpp>

#include <memory>
#include <tuple>

#include "er_connection.hpp"
#include "er_impl_common.hpp"
#include "er_network_variant.hpp"
#include "er_statement.hpp"
#include "get_endpoint.hpp"
#include "handler_call_tracker.hpp"
#include "network_result.hpp"
#include "streams.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;
using boost::mysql::execution_state;
using boost::mysql::field_view;
using boost::mysql::handshake_params;
using boost::mysql::resultset;
using boost::mysql::row_view;
using boost::mysql::rows_view;
using boost::mysql::string_view;

namespace {

template <class Callable>
auto impl(Callable&& cb) -> network_result<decltype(cb().get())>
{
    using R = decltype(cb().get());  // Callable returns a future<R>
    std::future<R> fut = cb();
    try
    {
        return network_result<R>(error_code(), wait_for_result(fut));
    }
    catch (const boost::system::system_error& err)
    {
        return network_result<R>(err.code());
    }
}

template <class Callable>
network_result<no_result> impl_no_result(Callable&& cb)
{
    std::future<void> fut = cb();
    try
    {
        wait_for_result(fut);
        return network_result<no_result>(error_code());
    }
    catch (const boost::system::system_error& err)
    {
        return network_result<no_result>(err.code());
    }
}

template <class Stream>
class default_completion_tokens_statement : public er_statement_base<Stream>
{
public:
    network_result<no_result> execute_tuple2(
        field_view param1,
        field_view param2,
        resultset& result
    ) override
    {
        return impl_no_result([&] {
            return this->obj().async_execute(std::make_tuple(param1, param2), result);
        });
    }
    network_result<no_result> start_execution_tuple2(
        field_view param1,
        field_view param2,
        execution_state& st
    ) override
    {
        return impl_no_result([&] {
            return this->obj().async_start_execution(std::make_tuple(param1, param2), st);
        });
    }
    network_result<no_result> start_execution_it(
        value_list_it params_first,
        value_list_it params_last,
        execution_state& st
    ) override
    {
        return impl_no_result([&] {
            return this->obj().async_start_execution(params_first, params_last, st);
        });
    }
    network_result<no_result> close() override
    {
        return impl_no_result([&] { return this->obj().async_close(); });
    }
};

template <class Stream>
class default_completion_tokens_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;

    network_result<no_result> physical_connect() override
    {
        return impl_no_result([&] {
            return this->conn_.stream().lowest_layer().async_connect(get_endpoint<Stream>());
        });
    }
    network_result<no_result> connect(const handshake_params& params) override
    {
        return impl_no_result([&] {
            return this->conn_.async_connect(get_endpoint<Stream>(), params);
        });
    }
    network_result<no_result> handshake(const handshake_params& params) override
    {
        return impl_no_result([&] { return this->conn_.async_handshake(params); });
    }
    network_result<no_result> query(string_view query, resultset& result) override
    {
        return impl_no_result([&] { return this->conn_.async_query(query, result); });
    }
    network_result<no_result> start_query(string_view query, execution_state& st) override
    {
        return impl_no_result([&] { return this->conn_.async_start_query(query, st); });
    }
    network_result<no_result> prepare_statement(string_view statement, er_statement& stmt) override
    {
        return impl_no_result([&] {
            return this->conn_.async_prepare_statement(statement, this->cast(stmt));
        });
    }
    network_result<row_view> read_one_row(execution_state& st) override
    {
        return impl([&] { return this->conn_.async_read_one_row(st); });
    }
    network_result<rows_view> read_some_rows(execution_state& st) override
    {
        return impl([&] { return this->conn_.async_read_some_rows(st); });
    }
    network_result<no_result> quit() override
    {
        return impl_no_result([&] { return this->conn_.async_quit(); });
    }
    network_result<no_result> close() override
    {
        return impl_no_result([&] { return this->conn_.async_close(); });
    }
};

template <class Stream>
class default_completion_tokens_variant : public er_network_variant_base<
                                              Stream,
                                              default_completion_tokens_connection,
                                              default_completion_tokens_statement>
{
public:
    const char* variant_name() const override { return "default_completion_tokens"; }
};

}  // namespace

void boost::mysql::test::add_default_completion_tokens(std::vector<er_network_variant*>& output)
{
    static default_completion_tokens_variant<tcp_ssl_future_socket> obj;
    output.push_back(&obj);
}