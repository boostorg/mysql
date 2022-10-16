//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include <boost/mysql/errc.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/execute_params.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement_base.hpp>

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
using boost::mysql::field_view;
using boost::mysql::handshake_params;
using boost::mysql::row_view;
using boost::mysql::rows_view;

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
class default_completion_tokens_resultset : public er_resultset_base<Stream>
{
public:
    network_result<row_view> read_one() override
    {
        return impl([&] { return this->obj().async_read_one(); });
    }
    network_result<rows_view> read_some() override
    {
        return impl([&] { return this->obj().async_read_some(); });
    }
    network_result<rows_view> read_all() override
    {
        return impl([&] { return this->obj().async_read_all(); });
    }
};

template <class Stream>
class default_completion_tokens_statement : public er_statement_base<Stream>
{
public:
    network_result<no_result> execute_params(
        const boost::mysql::execute_params<value_list_it>& params,
        er_resultset& result
    ) override
    {
        return impl_no_result([&] { return this->obj().async_execute(params, this->cast(result)); }
        );
    }
    network_result<no_result> execute_collection(
        const std::vector<field_view>& values,
        er_resultset& result
    ) override
    {
        return impl_no_result([&] { return this->obj().async_execute(values, this->cast(result)); }
        );
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

    network_result<no_result> physical_connect(er_endpoint kind) override
    {
        return impl_no_result([&] {
            return this->conn_.stream().lowest_layer().async_connect(get_endpoint<Stream>(kind));
        });
    }
    network_result<no_result> connect(er_endpoint kind, const handshake_params& params) override
    {
        return impl_no_result([&] {
            return this->conn_.async_connect(get_endpoint<Stream>(kind), params);
        });
    }
    network_result<no_result> handshake(const handshake_params& params) override
    {
        return impl_no_result([&] { return this->conn_.async_handshake(params); });
    }
    network_result<no_result> query(boost::string_view query, er_resultset& result) override
    {
        return impl_no_result([&] { return this->conn_.async_query(query, this->cast(result)); });
    }
    network_result<no_result> prepare_statement(boost::string_view statement, er_statement& stmt)
        override
    {
        return impl_no_result([&] {
            return this->conn_.async_prepare_statement(statement, this->cast(stmt));
        });
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
                                              default_completion_tokens_statement,
                                              default_completion_tokens_resultset>
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