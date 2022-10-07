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
#include "boost/mysql/handshake_params.hpp"
#include "boost/mysql/errc.hpp"
#include "boost/mysql/error.hpp"
#include "boost/mysql/execute_params.hpp"
#include "boost/mysql/statement_base.hpp"
#include "boost/mysql/row.hpp"
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/asio/spawn.hpp>
#include <memory>

using namespace boost::mysql::test;
using boost::mysql::row;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::field_view;
using boost::mysql::connection_params;

namespace {

template <class Callable>
using impl_result_type = decltype(std::declval<Callable>()(
    std::declval<boost::asio::yield_context>(),
    std::declval<error_info&>()
));

template <class IoObj, class Callable>
network_result<impl_result_type<Callable>> impl(IoObj& obj, Callable&& cb)
{
    using R = impl_result_type<Callable>;

    std::promise<network_result<R>> prom;

    boost::asio::spawn(obj.get_executor(), [&](boost::asio::yield_context yield) {
        error_code ec = boost::mysql::make_error_code(boost::mysql::errc::no);
        error_info info ("error_info not cleared properly");
        R result = cb(yield[ec], info);
        prom.set_value(network_result<R>(ec, std::move(info), std::move(result)));
    });

    return prom.get_future().get();
}

template <class Stream>
class async_coroutine_resultset : public er_resultset_base<Stream>
{
public:
    using er_resultset_base<Stream>::er_resultset_base;
    network_result<bool> read_one(row& output) override
    {
        return impl(this->r_, [&](boost::asio::yield_context yield, error_info& info) {
            return this->r_.async_read_one(output, info, yield);
        });
    }
    network_result<std::vector<row>> read_many(std::size_t count) override
    {
        return impl(this->r_, [&](boost::asio::yield_context yield, error_info& info) {
            return this->r_.async_read_many(count, info, yield);
        });
    }
    network_result<std::vector<row>> read_all() override
    {
        return impl(this->r_, [&](boost::asio::yield_context yield, error_info& info) {
            return this->r_.async_read_all(info, yield);
        });
    }
};

template <class Stream>
network_result<er_resultset_ptr> erase_network_result(network_result<boost::mysql::resultset_base<Stream>>&& r)
{
    return network_result<er_resultset_ptr> (
        r.err, 
        std::move(*r.info), 
        erase_resultset<async_coroutine_resultset>(std::move(r.value))
    );
}

template <class Stream>
class async_coroutine_statement : public er_statement_base<Stream>
{
public:
    using er_statement_base<Stream>::er_statement_base;
    using resultset_type = boost::mysql::resultset_base<Stream>;

    network_result<er_resultset_ptr> execute_params(
        const boost::mysql::execute_params<value_list_it>& params
    ) override
    {
        return erase_network_result(impl(this->stmt_, [&](boost::asio::yield_context yield, error_info& info) {
            return this->stmt_.async_execute(params, info, yield);
        }));
    }
    network_result<er_resultset_ptr> execute_container(
        const std::vector<field_view>& values
    ) override
    {
        return erase_network_result(impl(this->stmt_, [&](boost::asio::yield_context yield, error_info& info) {
            return this->stmt_.async_execute(values, info, yield);
        }));
    }
    network_result<no_result> close() override
    {
        return impl(this->stmt_, [&](boost::asio::yield_context yield, error_info& info) {
            this->stmt_.async_close(info, yield);
            return no_result();
        });
    }
};

template <class Stream>
network_result<er_statement_ptr> erase_network_result(network_result<boost::mysql::statement_base<Stream>>&& r)
{
    return network_result<er_statement_ptr> (
        r.err, 
        std::move(*r.info), 
        erase_statement<async_coroutine_statement>(std::move(r.value))
    );
}

template <class Stream>
class async_coroutine_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;
    using statement_type = boost::mysql::statement_base<Stream>;
    using resultset_type = boost::mysql::resultset_base<Stream>;

    network_result<no_result> physical_connect(er_endpoint kind) override
    {
        return impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            info.clear();
            this->conn_.next_layer().lowest_layer().async_connect(get_endpoint<Stream>(kind), yield);
            return no_result();
        });
    }
    network_result<no_result> connect(
        er_endpoint kind,
        const boost::mysql::connection_params& params
    ) override
    {
        return impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            this->conn_.async_connect(get_endpoint<Stream>(kind), params, info, yield);
            return no_result();
        });
    }
    network_result<no_result> handshake(const connection_params& params) override
    {
        return impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            this->conn_.async_handshake(params, info, yield);
            return no_result();
        });
    }
    network_result<er_resultset_ptr> query(boost::string_view query) override
    {
        return erase_network_result(impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            return this->conn_.async_query(query, info, yield);
        }));
    }
    network_result<er_statement_ptr> prepare_statement(boost::string_view statement) override
    {
        return erase_network_result(impl(this->conn_, [&](boost::asio::yield_context yield, error_info& info) {
            return this->conn_.async_prepare_statement(statement, info, yield);
        }));
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
class async_coroutine_variant : public er_network_variant_base<Stream, async_coroutine_connection>
{
public:
    const char* variant_name() const override { return "async_coroutine"; }
};

async_coroutine_variant<tcp_socket> tcp;
async_coroutine_variant<tcp_ssl_socket> tcp_ssl;

} // anon namespace

void boost::mysql::test::add_async_coroutine(
    std::vector<er_network_variant*>& output
)
{
    output.push_back(&tcp);
    output.push_back(&tcp_ssl);
}