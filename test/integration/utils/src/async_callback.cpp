//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "er_network_variant.hpp"
#include "er_connection.hpp"
#include "er_resultset.hpp"
#include "er_statement.hpp"
#include "network_result.hpp"
#include "streams.hpp"
#include "er_impl_common.hpp"
#include "get_endpoint.hpp"
#include "handler_call_tracker.hpp"
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/errc.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/execute_params.hpp>
#include <boost/mysql/statement_base.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/connection.hpp>
#include <memory>

using namespace boost::mysql::test;
using boost::mysql::row;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::field_view;
using boost::mysql::connection_params;

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
    error_info info ("error_info not cleared properly");
    cb(handler<R>{prom, info, call_tracker}, info);
    auto res = prom.get_future().get();

    return res;
}

template <class Stream>
class async_callback_resultset : public er_resultset_base<Stream>
{
public:
    using er_resultset_base<Stream>::er_resultset_base;
    network_result<bool> read_one(row& output) override
    {
        return impl<bool>([&](handler<bool> h, error_info& info) {
            return this->r_.async_read_one(output, info, std::move(h));
        });
    }
    network_result<std::vector<row>> read_many(std::size_t count) override
    {
        return impl<std::vector<row>>([&](handler<std::vector<row>> h, error_info& info) {
            return this->r_.async_read_many(count, info, std::move(h));
        });
    }
    network_result<std::vector<row>> read_all() override
    {
        return impl<std::vector<row>>([&](handler<std::vector<row>> h, error_info& info) {
            return this->r_.async_read_all(info, std::move(h));
        });
    }
};

template <class Stream>
network_result<er_resultset_ptr> erase_network_result(network_result<boost::mysql::resultset_base<Stream>>&& r)
{
    return network_result<er_resultset_ptr> (
        r.err, 
        std::move(*r.info), 
        erase_resultset<async_callback_resultset>(std::move(r.value))
    );
}

template <class Stream>
class async_callback_statement : public er_statement_base<Stream>
{
public:
    using er_statement_base<Stream>::er_statement_base;
    using resultset_type = boost::mysql::resultset_base<Stream>;

    network_result<er_resultset_ptr> execute_params(
        const boost::mysql::execute_params<value_list_it>& params
    ) override
    {
        return erase_network_result(impl<resultset_type>([&](handler<resultset_type> h, error_info& info) {
            return this->stmt_.async_execute(params, info, std::move(h));
        }));
    }
    network_result<er_resultset_ptr> execute_container(
        const std::vector<field_view>& values
    ) override
    {
        return erase_network_result(impl<resultset_type>([&](handler<resultset_type> h, error_info& info) {
            return this->stmt_.async_execute(values, info, std::move(h));
        }));
    }
    network_result<no_result> close() override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->stmt_.async_close(info, std::move(h));
        });
    }
};

template <class Stream>
network_result<er_statement_ptr> erase_network_result(network_result<boost::mysql::statement_base<Stream>>&& r)
{
    return network_result<er_statement_ptr> (
        r.err, 
        std::move(*r.info), 
        erase_statement<async_callback_statement>(std::move(r.value))
    );
}

template <class Stream>
class async_callback_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;
    using statement_type = boost::mysql::statement_base<Stream>;
    using resultset_type = boost::mysql::resultset_base<Stream>;

    network_result<no_result> physical_connect(er_endpoint kind) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            info.clear();
            return this->conn_.next_layer().lowest_layer().async_connect(get_endpoint<Stream>(kind), std::move(h));
        });
    }
    network_result<no_result> connect(
        er_endpoint kind,
        const boost::mysql::connection_params& params
    ) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->conn_.async_connect(get_endpoint<Stream>(kind), params, info, std::move(h));
        });
    }
    network_result<no_result> handshake(const connection_params& params) override
    {
        return impl<no_result>([&](handler<no_result> h, error_info& info) {
            return this->conn_.async_handshake(params, info, std::move(h));
        });
    }
    network_result<er_resultset_ptr> query(boost::string_view query) override
    {
        return erase_network_result(impl<resultset_type>([&](handler<resultset_type> h, error_info& info) {
            return this->conn_.async_query(query, info, std::move(h));
        }));
    }
    network_result<er_statement_ptr> prepare_statement(boost::string_view statement) override
    {
        return erase_network_result(impl<statement_type>([&](handler<statement_type> h, error_info& info) {
            return this->conn_.async_prepare_statement(statement, info, std::move(h));
        }));
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
class async_callback_variant : public er_network_variant_base<Stream, async_callback_connection>
{
public:
    const char* variant_name() const override { return "async_callback"; }
};

async_callback_variant<tcp_socket> tcp_variant;
async_callback_variant<tcp_ssl_socket> tcp_ssl_variant;
async_callback_variant<tcp_ssl_future_socket> def_compl_variant;
#if BOOST_ASIO_HAS_LOCAL_SOCKETS
async_callback_variant<unix_socket> unix_variant;
#endif

} // anon namespace

void boost::mysql::test::add_async_callback(
    std::vector<er_network_variant*>& output
)
{
    output.push_back(&tcp_variant);
    output.push_back(&tcp_ssl_variant);
    output.push_back(&def_compl_variant);
#if BOOST_ASIO_HAS_LOCAL_SOCKETS
    output.push_back(&unix_variant);
#endif
}