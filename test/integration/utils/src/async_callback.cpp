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
using boost::mysql::row;
using boost::mysql::row_view;
using boost::mysql::rows_view;

namespace {

template <class R>
struct handler
{
    std::promise<network_result<R>>& prom_;
    error_info& output_info_;
    handler_call_tracker& call_tracker_;

    // For operations with a return type
    void operator()(error_code code, R retval)
    {
        call_tracker_.register_call();
        prom_.set_value(network_result<R>(code, std::move(output_info_), std::move(retval)));
    }

    // For operations without a return type (R=no_result)
    void operator()(error_code code)
    {
        prom_.set_value(network_result<R>(code, std::move(output_info_)));
    }
};

template <class R, class Callable>
network_result<R> impl(Callable&& cb)
{
    handler_call_tracker call_tracker;
    std::promise<network_result<R>> prom;
    error_info info("error_info not cleared properly");
    cb(handler<R>{prom, info, call_tracker}, info);
    return wait_for_result(prom);
}

template <class Stream>
class async_callback_resultset : public er_resultset_base<Stream>
{
public:
    network_result<row_view> read_one() override
    {
        return impl<row_view>([&](handler<row_view> h, error_info& info) {
            return this->obj().async_read_one(info, std::move(h));
        });
    }
    network_result<rows_view> read_some() override
    {
        return impl<rows_view>([&](handler<rows_view> h, error_info& info) {
            return this->obj().async_read_some(info, std::move(h));
        });
    }
    network_result<rows_view> read_all() override
    {
        return impl<rows_view>([&](handler<rows_view> h, error_info& info) {
            return this->obj().async_read_all(info, std::move(h));
        });
    }
};

template <class Stream>
class async_callback_statement : public er_statement_base<Stream>
{
public:
    network_result<no_result> execute_params(
        const boost::mysql::execute_params<value_list_it>& params,
        er_resultset& result
    ) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->obj().async_execute(params, this->cast(result), info, std::move(h));
        });
    }
    network_result<no_result> execute_collection(
        const std::vector<field_view>& values,
        er_resultset& result
    ) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->obj().async_execute(values, this->cast(result), info, std::move(h));
        });
    }
    network_result<no_result> close() override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->obj().async_close(info, std::move(h));
        });
    }
};

template <class Stream>
class async_callback_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;

    network_result<no_result> physical_connect(er_endpoint kind) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            info.clear();
            return this->conn_.stream().lowest_layer().async_connect(
                get_endpoint<Stream>(kind),
                std::move(h)
            );
        });
    }
    network_result<no_result> connect(er_endpoint kind, const handshake_params& params) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->conn_
                .async_connect(get_endpoint<Stream>(kind), params, info, std::move(h));
        });
    }
    network_result<no_result> handshake(const handshake_params& params) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->conn_.async_handshake(params, info, std::move(h));
        });
    }
    network_result<no_result> query(boost::string_view query, er_resultset& result) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->conn_.async_query(query, this->cast(result), info, std::move(h));
        });
    }
    network_result<no_result> prepare_statement(boost::string_view statement, er_statement& stmt)
        override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->conn_
                .async_prepare_statement(statement, this->cast(stmt), info, std::move(h));
        });
    }
    network_result<no_result> quit() override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->conn_.async_quit(info, std::move(h));
        });
    }
    network_result<no_result> close() override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->conn_.async_close(info, std::move(h));
        });
    }
};

template <class Stream>
class async_callback_variant : public er_network_variant_base<
                                   Stream,
                                   async_callback_connection,
                                   async_callback_statement,
                                   async_callback_resultset>
{
public:
    const char* variant_name() const override { return "async_callback"; }
};

}  // namespace

void boost::mysql::test::add_async_callback(std::vector<er_network_variant*>& output)
{
    static async_callback_variant<tcp_socket> tcp_variant;
    static async_callback_variant<tcp_ssl_socket> tcp_ssl_variant;
    static async_callback_variant<tcp_ssl_future_socket> def_compl_variant;
#if BOOST_ASIO_HAS_LOCAL_SOCKETS
    static async_callback_variant<unix_socket> unix_variant;
#endif

    output.push_back(&tcp_variant);
    output.push_back(&tcp_ssl_variant);
    output.push_back(&def_compl_variant);
#if BOOST_ASIO_HAS_LOCAL_SOCKETS
    output.push_back(&unix_variant);
#endif
}