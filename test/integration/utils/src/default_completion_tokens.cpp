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
#include "boost/mysql/connection_params.hpp"
#include "boost/mysql/errc.hpp"
#include "boost/mysql/error.hpp"
#include "boost/mysql/execute_params.hpp"
#include "boost/mysql/prepared_statement.hpp"
#include "boost/mysql/row.hpp"
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/connection.hpp>
#include <memory>

using namespace boost::mysql::test;
using boost::mysql::row;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::value;
using boost::mysql::connection_params;

namespace {

template <class Callable>
auto impl(
    Callable&& cb
) -> network_result<decltype(cb().get())>
{
    using R = decltype(cb().get()); // Callable returns a future<R>
    std::future<R> fut = cb();
    try
    {
        return network_result<R>(
            error_code(),
            fut.get()
        );
    }
    catch (const boost::system::system_error& err)
    {
        return network_result<R>(err.code());
    }
}

template <class Callable>
network_result<no_result> impl_no_result(
    Callable&& cb
)
{
    std::future<void> fut = cb();
    try
    {
        fut.get();
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
    using er_resultset_base<Stream>::er_resultset_base;
    network_result<bool> read_one(row& output) override
    {
        return impl([&] {
            return this->r_.async_read_one(output);
        });
    }
    network_result<std::vector<row>> read_many(std::size_t count) override
    {
        return impl([&] {
            return this->r_.async_read_many(count);
        });
    }
    network_result<std::vector<row>> read_all() override
    {
        return impl([&] {
            return this->r_.async_read_all();
        });
    }
};

template <class Stream>
network_result<er_resultset_ptr> erase_network_result(
    network_result<boost::mysql::resultset<Stream>>&& r
)
{
    return network_result<er_resultset_ptr> (
        r.err, 
        erase_resultset<default_completion_tokens_resultset>(std::move(r.value))
    );
}

template <class Stream>
class default_completion_tokens_statement : public er_statement_base<Stream>
{
public:
    using er_statement_base<Stream>::er_statement_base;
    using resultset_type = boost::mysql::resultset<Stream>;

    network_result<er_resultset_ptr> execute_params(
        const boost::mysql::execute_params<value_list_it>& params
    ) override
    {
        return erase_network_result(impl([&] {
            return this->stmt_.async_execute(params);
        }));
    }
    network_result<er_resultset_ptr> execute_container(
        const std::vector<value>& values
    ) override
    {
        return erase_network_result(impl([&] {
            return this->stmt_.async_execute(values);
        }));
    }
    network_result<no_result> close() override
    {
        return impl_no_result([&] {
            return this->stmt_.async_close();
        });
    }
};

template <class Stream>
network_result<er_statement_ptr> erase_network_result(
    network_result<boost::mysql::prepared_statement<Stream>>&& r
)
{
    return network_result<er_statement_ptr> (
        r.err, 
        erase_statement<default_completion_tokens_statement>(std::move(r.value))
    );
}

template <class Stream>
class default_completion_tokens_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;
    using statement_type = boost::mysql::prepared_statement<Stream>;
    using resultset_type = boost::mysql::resultset<Stream>;

    network_result<no_result> physical_connect(er_endpoint kind) override
    {
        return impl_no_result([&] {
            return this->conn_.next_layer().lowest_layer().async_connect(get_endpoint<Stream>(kind));
        });
    }
    network_result<no_result> connect(
        er_endpoint kind,
        const boost::mysql::connection_params& params
    ) override
    {
        return impl_no_result([&] {
            return this->conn_.async_connect(get_endpoint<Stream>(kind), params);
        });
    }
    network_result<no_result> handshake(const connection_params& params) override
    {
        return impl_no_result([&] {
            return this->conn_.async_handshake(params);
        });
    }
    network_result<er_resultset_ptr> query(boost::string_view query) override
    {
        return erase_network_result(impl([&] {
            return this->conn_.async_query(query);
        }));
    }
    network_result<er_statement_ptr> prepare_statement(boost::string_view statement) override
    {
        return erase_network_result(impl([&] {
            return this->conn_.async_prepare_statement(statement);
        }));
    }
    network_result<no_result> quit() override
    {
        return impl_no_result([&] {
            return this->conn_.async_quit();
        });
    }
    network_result<no_result> close() override
    {
        return impl_no_result([&] {
            return this->conn_.async_close();
        });
    }
};

template <class Stream>
class default_completion_tokens_variant : 
    public er_network_variant_base<Stream, default_completion_tokens_connection>
{
public:
    const char* variant_name() const override { return "default_completion_tokens"; }
};

default_completion_tokens_variant<tcp_ssl_future_socket> obj;

} // anon namespace

void boost::mysql::test::add_default_completion_tokens(
    std::vector<er_network_variant*>& output
)
{
    output.push_back(&obj);
}