//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_CONNECTION_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_CONNECTION_HPP

#include <boost/mysql/error.hpp>
#include <boost/mysql/execute_params.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement_base.hpp>

#include <memory>

#include "er_endpoint.hpp"
#include "network_result.hpp"

namespace boost {
namespace mysql {
namespace test {

class er_connection
{
public:
    virtual ~er_connection() {}
    virtual bool valid() const = 0;
    virtual bool uses_ssl() const = 0;
    virtual bool is_open() const = 0;
    virtual network_result<no_result> physical_connect(er_endpoint) = 0;
    virtual network_result<no_result> connect(er_endpoint, const handshake_params&) = 0;
    virtual network_result<no_result> handshake(const handshake_params&) = 0;
    virtual network_result<no_result> query(boost::string_view query, resultset_base& result) = 0;
    virtual network_result<no_result> prepare_statement(
        boost::string_view statement,
        statement_base& stmt
    ) = 0;
    virtual network_result<no_result> execute_statement(
        statement_base& stmt,
        const execute_params<const field_view*>& params,
        resultset_base& result
    ) = 0;
    virtual network_result<no_result> close_statement(statement_base& stmt) = 0;
    virtual network_result<row_view> read_one_row(resultset_base& stmt) = 0;
    virtual network_result<rows_view> read_some_rows(resultset_base& stmt) = 0;
    virtual network_result<rows_view> read_all_rows(resultset_base& stmt) = 0;
    virtual network_result<no_result> quit_connection() = 0;
    virtual network_result<no_result> close_connection() = 0;
    virtual void sync_close() noexcept = 0;  // used by fixture cleanup functions
};

using er_connection_ptr = std::unique_ptr<er_connection>;

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
