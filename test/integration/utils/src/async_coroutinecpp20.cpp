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
#include <boost/mysql/statement_base.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>

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
using boost::mysql::server_diagnostics;
using boost::mysql::string_view;

#ifdef BOOST_ASIO_HAS_CO_AWAIT

using boost::asio::use_awaitable;

namespace {

template <class Callable>
using impl_result_type = typename decltype(std::declval<Callable>(
)(std::declval<server_diagnostics&>()))::value_type;

template <class IoObj, class Callable>
network_result<impl_result_type<Callable>> impl(IoObj& obj, Callable&& cb)
{
    using R = impl_result_type<Callable>;
    std::promise<network_result<R>> prom;

    boost::asio::co_spawn(
        obj.get_executor(),
        [&]() -> boost::asio::awaitable<void> {
            server_diagnostics info("server_diagnostics not cleared properly");
            try
            {
                // Create the task
                auto aw = cb(info);

                // Verify that initiation didn't have side effects
                BOOST_TEST(info.message() == "server_diagnostics not cleared properly");

                // Run task and verify results
                R result = co_await std::move(aw);
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
            server_diagnostics info("server_diagnostics not cleared properly");
            try
            {
                // Create the task
                auto aw = cb(info);

                // Verify that initiation didn't have side effects
                BOOST_TEST(info.message() == "server_diagnostics not cleared properly");

                // Run task and verify results
                co_await std::move(aw);
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
class async_coroutinecpp20_statement : public er_statement_base<Stream>
{
public:
    network_result<no_result> execute_tuple2(
        field_view param1,
        field_view param2,
        resultset& result
    ) override
    {
        return impl_no_result(this->obj(), [&](server_diagnostics& info) {
            return this->obj()
                .async_execute(std::make_tuple(param1, param2), result, info, use_awaitable);
        });
    }
    network_result<no_result> start_execution_tuple2(
        field_view param1,
        field_view param2,
        execution_state& st
    ) override
    {
        return impl_no_result(this->obj(), [&](server_diagnostics& info) {
            return this->obj()
                .async_start_execution(std::make_tuple(param1, param2), st, info, use_awaitable);
        });
    }
    network_result<no_result> start_execution_it(
        value_list_it params_first,
        value_list_it params_last,
        execution_state& st
    ) override
    {
        return impl_no_result(this->obj(), [&](server_diagnostics& info) {
            return this->obj()
                .async_start_execution(params_first, params_last, st, info, use_awaitable);
        });
    }
    network_result<no_result> close() override
    {
        return impl_no_result(this->obj(), [&](server_diagnostics& info) {
            return this->obj().async_close(info, use_awaitable);
        });
    }
};

template <class Stream>
class async_coroutinecpp20_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;

    network_result<no_result> physical_connect() override
    {
        return impl_no_result(
            this->conn_,
            [&](server_diagnostics& info) -> boost::asio::awaitable<void> {
                info.clear();

                co_await this->conn_.stream().lowest_layer().async_connect(
                    get_endpoint<Stream>(),
                    use_awaitable
                );
            }
        );
    }
    network_result<no_result> connect(const handshake_params& params) override
    {
        return impl_no_result(this->conn_, [&](server_diagnostics& info) {
            return this->conn_.async_connect(get_endpoint<Stream>(), params, info, use_awaitable);
        });
    }
    network_result<no_result> handshake(const handshake_params& params) override
    {
        return impl_no_result(this->conn_, [&](server_diagnostics& info) {
            return this->conn_.async_handshake(params, info, use_awaitable);
        });
    }
    network_result<no_result> query(string_view query, resultset& result) override
    {
        return impl_no_result(this->conn_, [&](server_diagnostics& info) {
            return this->conn_.async_query(query, result, info, use_awaitable);
        });
    }
    network_result<no_result> start_query(string_view query, execution_state& st) override
    {
        return impl_no_result(this->conn_, [&](server_diagnostics& info) {
            return this->conn_.async_start_query(query, st, info, use_awaitable);
        });
    }
    network_result<no_result> prepare_statement(string_view statement, er_statement& stmt) override
    {
        return impl_no_result(this->conn_, [&](server_diagnostics& info) {
            return this->conn_
                .async_prepare_statement(statement, this->cast(stmt), info, use_awaitable);
        });
    }
    network_result<row_view> read_one_row(execution_state& st) override
    {
        return impl(this->conn_, [&](server_diagnostics& info) {
            return this->conn_.async_read_one_row(st, info, use_awaitable);
        });
    }
    network_result<rows_view> read_some_rows(execution_state& st) override
    {
        return impl(this->conn_, [&](server_diagnostics& info) {
            return this->conn_.async_read_some_rows(st, info, use_awaitable);
        });
    }
    network_result<no_result> quit() override
    {
        return impl_no_result(this->conn_, [&](server_diagnostics& info) {
            return this->conn_.async_quit(info, use_awaitable);
        });
    }
    network_result<no_result> close() override
    {
        return impl_no_result(this->conn_, [&](server_diagnostics& info) {
            return this->conn_.async_close(info, use_awaitable);
        });
    }
};

template <class Stream>
class async_coroutinecpp20_variant : public er_network_variant_base<
                                         Stream,
                                         async_coroutinecpp20_connection,
                                         async_coroutinecpp20_statement>
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
