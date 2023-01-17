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

template <class R>
struct handler
{
    std::promise<network_result<R>>& prom_;
    handler_call_tracker& call_tracker_;

    // For operations with a return type
    void operator()(error_code code, R retval)
    {
        call_tracker_.register_call();
        prom_.set_value(network_result<R>(code, std::move(retval)));
    }

    // For operations without a return type (R=no_result)
    void operator()(error_code code)
    {
        call_tracker_.register_call();
        prom_.set_value(network_result<R>(code));
    }
};

template <class R, class Callable>
network_result<R> impl(Callable&& cb)
{
    handler_call_tracker call_tracker;
    std::promise<network_result<R>> prom;
    cb(handler<R>{prom, call_tracker});
    return wait_for_result(prom);
}

template <class Stream>
class async_callback_noerrinfo_statement : public er_statement_base<Stream>
{
public:
    network_result<no_result> execute_tuple2(
        field_view param1,
        field_view param2,
        resultset& result
    ) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->obj().async_execute(std::make_tuple(param1, param2), result, std::move(h));
        });
    }
    network_result<no_result> start_execution_tuple2(
        field_view param1,
        field_view param2,
        execution_state& st
    ) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->obj()
                .async_start_execution(std::make_tuple(param1, param2), st, std::move(h));
        });
    }
    network_result<no_result> start_execution_it(
        value_list_it params_first,
        value_list_it params_last,
        execution_state& st
    ) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->obj().async_start_execution(params_first, params_last, st, std::move(h));
        });
    }
    network_result<no_result> close() override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->obj().async_close(std::move(h));
        });
    }
};

template <class Stream>
class async_callback_noerrinfo_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;

    network_result<no_result> physical_connect() override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->conn_.stream().lowest_layer().async_connect(
                get_endpoint<Stream>(),
                std::move(h)
            );
        });
    }
    network_result<no_result> connect(const handshake_params& params) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->conn_.async_connect(get_endpoint<Stream>(), params, std::move(h));
        });
    }
    network_result<no_result> handshake(const handshake_params& params) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->conn_.async_handshake(params, std::move(h));
        });
    }
    network_result<no_result> query(string_view query, resultset& result) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->conn_.async_query(query, result, std::move(h));
        });
    }
    network_result<no_result> start_query(string_view query, execution_state& st) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->conn_.async_start_query(query, st, std::move(h));
        });
    }
    network_result<no_result> prepare_statement(string_view statement, er_statement& stmt) override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->conn_.async_prepare_statement(statement, this->cast(stmt), std::move(h));
        });
    }
    network_result<row_view> read_one_row(execution_state& st) override
    {
        return impl<row_view>([&](handler<row_view> h) {
            return this->conn_.async_read_one_row(st, std::move(h));
        });
    }
    network_result<rows_view> read_some_rows(execution_state& st) override
    {
        return impl<rows_view>([&](handler<rows_view> h) {
            return this->conn_.async_read_some_rows(st, std::move(h));
        });
    }
    network_result<no_result> quit() override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->conn_.async_quit(std::move(h));
        });
    }
    network_result<no_result> close() override
    {
        return impl<no_result>([&](handler<no_result> h) {
            return this->conn_.async_close(std::move(h));
        });
    }
};

template <class Stream>
class async_callback_noerrinfo_variant : public er_network_variant_base<
                                             Stream,
                                             async_callback_noerrinfo_connection,
                                             async_callback_noerrinfo_statement>
{
public:
    const char* variant_name() const override { return "async_callback_noerrinfo"; }
};

}  // namespace

void boost::mysql::test::add_async_callback_noerrinfo(std::vector<er_network_variant*>& output)
{
    static async_callback_noerrinfo_variant<tcp_socket> tcp;
    output.push_back(&tcp);
}