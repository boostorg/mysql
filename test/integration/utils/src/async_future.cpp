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
#include <memory>

using namespace boost::mysql::test;
using boost::mysql::row;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::field_view;
using boost::mysql::connection_params;

namespace {

template <class Callable>
auto impl(
    Callable&& cb
) -> network_result<decltype(cb(std::declval<error_info&>()).get())>
{
    using R = decltype(cb(std::declval<error_info&>()).get()); // Callable returns a future<R>
    error_info info ("Error info was not clearer properly");
    std::future<R> fut = cb(info);
    try
    {
        R res = fut.get();
        return network_result<R>(
            error_code(),
            std::move(info),
            std::move(res)
        );
    }
    catch (const boost::system::system_error& err)
    {
        return network_result<R>(err.code(), std::move(info));
    }
}

template <class Callable>
network_result<no_result> impl_no_result(
    Callable&& cb
)
{
    error_info info ("Error info was not clearer properly");
    std::future<void> fut = cb(info);
    try
    {
        fut.get();
        return network_result<no_result>(error_code(), std::move(info));
    }
    catch (const boost::system::system_error& err)
    {
        return network_result<no_result>(err.code(), std::move(info));
    }
}

template <class Stream>
class async_future_resultset : public er_resultset_base<Stream>
{
public:
    using er_resultset_base<Stream>::er_resultset_base;
    network_result<bool> read_one(row& output) override
    {
        return impl([&] (error_info& output_info) {
            return this->r_.async_read_one(output, output_info, boost::asio::use_future);
        });
    }
    network_result<std::vector<row>> read_many(std::size_t count) override
    {
        return impl([&] (error_info& output_info) {
            return this->r_.async_read_many(count, output_info, boost::asio::use_future);
        });
    }
    network_result<std::vector<row>> read_all() override
    {
        return impl([&] (error_info& output_info) {
            return this->r_.async_read_all(output_info, boost::asio::use_future);
        });
    }
};

template <class Stream>
network_result<er_resultset_ptr> erase_network_result(network_result<boost::mysql::resultset_base<Stream>>&& r)
{
    return network_result<er_resultset_ptr> (
        r.err, 
        std::move(*r.info), 
        erase_resultset<async_future_resultset>(std::move(r.value))
    );
}

template <class Stream>
class async_future_statement : public er_statement_base<Stream>
{
public:
    using er_statement_base<Stream>::er_statement_base;
    using resultset_type = boost::mysql::resultset_base<Stream>;

    network_result<er_resultset_ptr> execute_params(
        const boost::mysql::execute_params<value_list_it>& params
    ) override
    {
        return erase_network_result(impl([&] (error_info& output_info) {
            return this->stmt_.async_execute(params, output_info, boost::asio::use_future);
        }));
    }
    network_result<er_resultset_ptr> execute_container(
        const std::vector<field_view>& values
    ) override
    {
        return erase_network_result(impl([&] (error_info& output_info) {
            return this->stmt_.async_execute(values, output_info, boost::asio::use_future);
        }));
    }
    network_result<no_result> close() override
    {
        return impl_no_result([&] (error_info& output_info) {
            return this->stmt_.async_close(output_info, boost::asio::use_future);
        });
    }
};

template <class Stream>
network_result<er_statement_ptr> erase_network_result(network_result<boost::mysql::statement_base<Stream>>&& r)
{
    return network_result<er_statement_ptr> (
        r.err, 
        std::move(*r.info), 
        erase_statement<async_future_statement>(std::move(r.value))
    );
}

template <class Stream>
class async_future_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;
    using statement_type = boost::mysql::statement_base<Stream>;
    using resultset_type = boost::mysql::resultset_base<Stream>;

    network_result<no_result> physical_connect(er_endpoint kind) override
    {
        return impl_no_result([&] (error_info& output_info) {
            output_info.clear();
            return this->conn_.next_layer().lowest_layer().async_connect(get_endpoint<Stream>(kind), boost::asio::use_future);
        });
    }
    network_result<no_result> connect(
        er_endpoint kind,
        const boost::mysql::connection_params& params
    ) override
    {
        return impl_no_result([&] (error_info& output_info) {
            return this->conn_.async_connect(get_endpoint<Stream>(kind), params, output_info, boost::asio::use_future);
        });
    }
    network_result<no_result> handshake(const connection_params& params) override
    {
        return impl_no_result([&] (error_info& output_info) {
            return this->conn_.async_handshake(params, output_info, boost::asio::use_future);
        });
    }
    network_result<er_resultset_ptr> query(boost::string_view query) override
    {
        return erase_network_result(impl([&] (error_info& output_info) {
            return this->conn_.async_query(query, output_info, boost::asio::use_future);
        }));
    }
    network_result<er_statement_ptr> prepare_statement(boost::string_view statement) override
    {
        return erase_network_result(impl([&] (error_info& output_info) {
            return this->conn_.async_prepare_statement(statement, output_info, boost::asio::use_future);
        }));
    }
    network_result<no_result> quit() override
    {
        return impl_no_result([&] (error_info& output_info) {
            return this->conn_.async_quit(output_info, boost::asio::use_future);
        });
    }
    network_result<no_result> close() override
    {
        return impl_no_result([&] (error_info& output_info) {
            return this->conn_.async_close(output_info, boost::asio::use_future);
        });
    }
};

template <class Stream>
class async_future_variant : public er_network_variant_base<Stream, async_future_connection>
{
public:
    const char* variant_name() const override { return "async_future"; }
};

async_future_variant<tcp_socket> tcp;
async_future_variant<tcp_ssl_socket> tcp_ssl;

} // anon namespace

void boost::mysql::test::add_async_future(
    std::vector<er_network_variant*>& output
)
{
    output.push_back(&tcp);
    output.push_back(&tcp_ssl);
}