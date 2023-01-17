//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/connection.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/server_diagnostics.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/statement_base.hpp>
#include <boost/mysql/string_view.hpp>

#include <memory>

#include "er_connection.hpp"
#include "er_impl_common.hpp"
#include "er_network_variant.hpp"
#include "er_statement.hpp"
#include "get_endpoint.hpp"
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
using boost::mysql::server_diagnostics;
using boost::mysql::string_view;

namespace {

template <class Callable>
using impl_result_type = decltype(std::declval<Callable>(
)(std::declval<error_code&>(), std::declval<server_diagnostics&>()));

template <class Callable>
static network_result<impl_result_type<Callable>> impl(Callable&& cb)
{
    network_result<impl_result_type<Callable>> res(
        boost::mysql::server_errc::no,
        server_diagnostics("server_diagnostics not cleared properly")
    );
    res.value = cb(res.err, *res.diag);
    return res;
}

template <class Stream>
class sync_errc_statement : public er_statement_base<Stream>
{
public:
    network_result<no_result> execute_tuple2(
        field_view param1,
        field_view param2,
        resultset& result
    ) override
    {
        return impl([&](error_code& err, server_diagnostics& diag) {
            this->obj().execute(std::make_tuple(param1, param2), result, err, diag);
            return no_result();
        });
    }
    network_result<no_result> start_execution_tuple2(
        field_view param1,
        field_view param2,
        execution_state& st
    ) override
    {
        return impl([&](error_code& err, server_diagnostics& info) {
            this->obj().start_execution(std::make_tuple(param1, param2), st, err, info);
            return no_result();
        });
    }
    network_result<no_result> start_execution_it(
        value_list_it params_first,
        value_list_it params_last,
        execution_state& st
    ) override
    {
        return impl([&](error_code& err, server_diagnostics& info) {
            this->obj().start_execution(params_first, params_last, st, err, info);
            return no_result();
        });
    }
    network_result<no_result> close() override
    {
        return impl([&](error_code& code, server_diagnostics& info) {
            this->obj().close(code, info);
            return no_result();
        });
    }
};

template <class Stream>
class sync_errc_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;

    network_result<no_result> physical_connect() override
    {
        return impl([&](error_code& code, server_diagnostics& info) {
            info.clear();
            this->conn_.stream().lowest_layer().connect(get_endpoint<Stream>(), code);
            return no_result();
        });
    }
    network_result<no_result> connect(const handshake_params& params) override
    {
        return impl([&](error_code& code, server_diagnostics& info) {
            this->conn_.connect(get_endpoint<Stream>(), params, code, info);
            return no_result();
        });
    }
    network_result<no_result> handshake(const handshake_params& params) override
    {
        return impl([&](error_code& code, server_diagnostics& info) {
            this->conn_.handshake(params, code, info);
            return no_result();
        });
    }
    network_result<no_result> query(string_view query, resultset& result) override
    {
        return impl([&](error_code& code, server_diagnostics& info) {
            this->conn_.query(query, result, code, info);
            return no_result();
        });
    }
    network_result<no_result> start_query(string_view query, execution_state& st) override
    {
        return impl([&](error_code& code, server_diagnostics& info) {
            this->conn_.start_query(query, st, code, info);
            return no_result();
        });
    }
    network_result<no_result> prepare_statement(string_view statement, er_statement& stmt) override
    {
        return impl([&](error_code& err, server_diagnostics& info) {
            this->conn_.prepare_statement(statement, this->cast(stmt), err, info);
            return no_result();
        });
    }
    network_result<row_view> read_one_row(execution_state& st) override
    {
        return impl([&](error_code& code, server_diagnostics& info) {
            return this->conn_.read_one_row(st, code, info);
        });
    }
    network_result<rows_view> read_some_rows(execution_state& st) override
    {
        return impl([&](error_code& code, server_diagnostics& info) {
            return this->conn_.read_some_rows(st, code, info);
        });
    }
    network_result<no_result> quit() override
    {
        return impl([&](error_code& code, server_diagnostics& info) {
            this->conn_.quit(code, info);
            return no_result();
        });
    }
    network_result<no_result> close() override
    {
        return impl([&](error_code& code, server_diagnostics& info) {
            this->conn_.close(code, info);
            return no_result();
        });
    }
};

template <class Stream>
class sync_errc_variant
    : public er_network_variant_base<Stream, sync_errc_connection, sync_errc_statement>
{
public:
    const char* variant_name() const override { return "sync_errc"; }
};

}  // namespace

void boost::mysql::test::add_sync_errc(std::vector<er_network_variant*>& output)
{
    static sync_errc_variant<tcp_socket> tcp_variant;
    static sync_errc_variant<tcp_ssl_socket> tcp_ssl_variant;
#if BOOST_ASIO_HAS_LOCAL_SOCKETS
    static sync_errc_variant<unix_socket> unix_variant;
    static sync_errc_variant<unix_ssl_socket> unix_ssl_variant;
#endif

    output.push_back(&tcp_variant);
    output.push_back(&tcp_ssl_variant);
#if BOOST_ASIO_HAS_LOCAL_SOCKETS
    output.push_back(&unix_variant);
    output.push_back(&unix_ssl_variant);
#endif
}
