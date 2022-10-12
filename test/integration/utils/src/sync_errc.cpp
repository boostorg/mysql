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
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/statement_base.hpp>

#include <memory>

#include "er_connection.hpp"
#include "er_impl_common.hpp"
#include "er_network_variant.hpp"
#include "get_endpoint.hpp"
#include "network_result.hpp"
#include "streams.hpp"

using namespace boost::mysql::test;
using boost::mysql::errc;
using boost::mysql::error_code;
using boost::mysql::error_info;
using boost::mysql::execute_params;
using boost::mysql::field_view;
using boost::mysql::handshake_params;
using boost::mysql::resultset_base;
using boost::mysql::row_view;
using boost::mysql::rows_view;
using boost::mysql::statement_base;

namespace {

template <class Callable>
using impl_result_type = decltype(std::declval<Callable>(
)(std::declval<error_code&>(), std::declval<error_info&>()));

template <class Callable>
static network_result<impl_result_type<Callable>> impl(Callable&& cb)
{
    network_result<impl_result_type<Callable>> res(
        boost::mysql::make_error_code(errc::no),
        error_info("error_info not cleared properly")
    );
    res.value = cb(res.err, *res.info);
    return res;
}

template <class Stream>
class sync_errc_connection : public er_connection_base<Stream>
{
public:
    using er_connection_base<Stream>::er_connection_base;
    using statement_type = boost::mysql::statement<Stream>;
    using resultset_type = boost::mysql::resultset<Stream>;

    static statement_type& cast_statement(statement_base& stmt) noexcept
    {
        return static_cast<statement_type&>(stmt);
    }
    static resultset_type& cast_resultset(resultset_base& stmt) noexcept
    {
        return static_cast<resultset_type&>(stmt);
    }

    network_result<no_result> physical_connect(er_endpoint kind) override
    {
        return impl([&](error_code& code, error_info& info) {
            info.clear();
            this->conn_.stream().lowest_layer().connect(get_endpoint<Stream>(kind), code);
            return no_result();
        });
    }
    network_result<no_result> connect(
        er_endpoint kind,
        const boost::mysql::handshake_params& params
    ) override
    {
        return impl([&](error_code& code, error_info& info) {
            this->conn_.connect(get_endpoint<Stream>(kind), params, code, info);
            return no_result();
        });
    }
    network_result<no_result> handshake(const handshake_params& params) override
    {
        return impl([&](error_code& code, error_info& info) {
            this->conn_.handshake(params, code, info);
            return no_result();
        });
    }
    network_result<no_result> query(boost::string_view query, resultset_base& result) override
    {
        return impl([&](error_code& code, error_info& info) {
            this->conn_.query(query, cast_resultset(result), code, info);
            return no_result();
        });
    }
    network_result<no_result> prepare_statement(boost::string_view statement, statement_base& stmt)
        override
    {
        return impl([&](error_code& err, error_info& info) {
            this->conn_.prepare_statement(statement, cast_statement(stmt), err, info);
            return no_result();
        });
    }
    network_result<no_result> execute_statement(
        statement_base& stmt,
        const execute_params<const field_view*>& params,
        resultset_base& result
    ) override
    {
        return impl([&](error_code& err, error_info& info) {
            cast_statement(stmt).execute(params, cast_resultset(result), err, info);
            return no_result();
        });
    }
    network_result<no_result> close_statement(statement_base& stmt) override
    {
        return impl([&](error_code& code, error_info& info) {
            cast_statement(stmt).close(code, info);
            return no_result();
        });
    }
    network_result<row_view> read_one_row(resultset_base& result) override
    {
        return impl([&](error_code& code, error_info& info) {
            return cast_resultset(result).read_one(code, info);
        });
    }
    network_result<rows_view> read_some_rows(resultset_base& result) override
    {
        return impl([&](error_code& code, error_info& info) {
            return cast_resultset(result).read_some(code, info);
        });
    }
    network_result<rows_view> read_all_rows(resultset_base& result) override
    {
        return impl([&](error_code& code, error_info& info) {
            return cast_resultset(result).read_all(code, info);
        });
    }
    network_result<no_result> quit_connection() override
    {
        return impl([&](error_code& code, error_info& info) {
            this->conn_.quit(code, info);
            return no_result();
        });
    }
    network_result<no_result> close_connection() override
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

}  // namespace

void boost::mysql::test::add_sync_errc(std::vector<er_network_variant*>& output)
{
    output.push_back(&tcp_variant);
    output.push_back(&tcp_ssl_variant);
    output.push_back(&def_compl_variant);
#if BOOST_ASIO_HAS_LOCAL_SOCKETS
    output.push_back(&unix_variant);
#endif
}
