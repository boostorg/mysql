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
using boost::mysql::errc;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::field_view;
using boost::mysql::connection_params;

namespace {

template <class Callable>
using impl_result_type = decltype(std::declval<Callable>()(
    std::declval<error_code&>(),
    std::declval<error_info&>()
));

template <class Callable>
static network_result<impl_result_type<Callable>> impl(Callable&& cb)
{
    network_result<impl_result_type<Callable>> res (
        boost::mysql::make_error_code(errc::no),
        error_info("error_info not cleared properly")
    );
    res.value = cb(res.err, *res.info);
    return res;
}

template <class Stream>
class sync_errc_resultset : public er_resultset_base<Stream>
{
public:
    using er_resultset_base<Stream>::er_resultset_base;
    network_result<bool> read_one(row& output) override
    {
        return impl([&](error_code& code, error_info& info) {
            return this->r_.read_one(output, code, info);
        });
    }
    network_result<std::vector<row>> read_many(std::size_t count) override
    {
        return impl([&](error_code& code, error_info& info) {
            return this->r_.read_many(count, code, info);
        });
    }
    network_result<std::vector<row>> read_all() override
    {
        return impl([&](error_code& code, error_info& info) {
            return this->r_.read_all(code, info);
        });
    }
};

template <class Stream>
class sync_errc_statement : public er_statement_base<Stream>
{
public:
    using er_statement_base<Stream>::er_statement_base;
    network_result<er_resultset_ptr> execute_params(
        const boost::mysql::execute_params<value_list_it>& params
    ) override
    {
        return impl([&](error_code& err, error_info& info) {
            return erase_resultset<sync_errc_resultset>(this->stmt_.execute(params, err, info));
        });
    }
    network_result<er_resultset_ptr> execute_container(
        const std::vector<field_view>& values
    ) override
    {
        return impl([&](error_code& err, error_info& info) {
            return erase_resultset<sync_errc_resultset>(this->stmt_.execute(values, err, info));
        });
    }
    network_result<no_result> close() override
    {
        return impl([&](error_code& code, error_info& info) {
            this->stmt_.close(code, info);
            return no_result();
        });
    }
};

template <class Stream>
class sync_errc_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;
    network_result<no_result> physical_connect(er_endpoint kind) override
    {
        return impl([&](error_code& code, error_info& info) {
            info.clear();
            this->conn_.next_layer().lowest_layer().connect(get_endpoint<Stream>(kind), code);
            return no_result();
        });
    }
    network_result<no_result> connect(
        er_endpoint kind,
        const boost::mysql::connection_params& params
    ) override
    {
        return impl([&](error_code& code, error_info& info) {
            this->conn_.connect(get_endpoint<Stream>(kind), params, code, info);
            return no_result();
        });
    }
    network_result<no_result> handshake(const connection_params& params) override
    {
        return impl([&](error_code& code, error_info& info) {
            this->conn_.handshake(params, code, info);
            return no_result();
        });
    }
    network_result<er_resultset_ptr> query(boost::string_view query) override
    {
        return impl([&](error_code& code, error_info& info) {
            return erase_resultset<sync_errc_resultset>(this->conn_.query(query, code, info));
        });
    }
    network_result<er_statement_ptr> prepare_statement(boost::string_view statement) override
    {
        return impl([&](error_code& err, error_info& info) {
            return erase_statement<sync_errc_statement>(this->conn_.prepare_statement(statement, err, info));
        });
    }
    network_result<no_result> quit() override
    {
        return impl([&](error_code& code, error_info& info) {
            this->conn_.quit(code, info);
            return no_result();
        });
    }
    network_result<no_result> close() override
    {
        return impl([&](error_code& code, error_info& info) {
            this->conn_.close(code, info);
            return no_result();
        });
    }
};

template <class Stream>
class sync_errc_variant : public er_network_variant_base<Stream, sync_errc_connection>
{
public:
    const char* variant_name() const override { return "sync_errc"; }
};

sync_errc_variant<tcp_socket> tcp_variant;
sync_errc_variant<tcp_ssl_socket> tcp_ssl_variant;
sync_errc_variant<tcp_ssl_future_socket> def_compl_variant;
#if BOOST_ASIO_HAS_LOCAL_SOCKETS
sync_errc_variant<unix_socket> unix_variant;
#endif

} // anon namespace

void boost::mysql::test::add_sync_errc(
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