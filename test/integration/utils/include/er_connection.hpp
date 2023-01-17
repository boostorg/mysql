//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_CONNECTION_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_CONNECTION_HPP

#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>

#include <memory>

#include "er_statement.hpp"
#include "network_result.hpp"

namespace boost {
namespace mysql {
namespace test {

class er_network_variant;

class er_connection
{
public:
    virtual ~er_connection() {}
    virtual bool uses_ssl() const = 0;
    virtual bool is_open() const = 0;
    virtual network_result<no_result> physical_connect() = 0;
    virtual network_result<no_result> connect(const handshake_params&) = 0;
    virtual network_result<no_result> handshake(const handshake_params&) = 0;
    virtual network_result<no_result> query(string_view query, resultset& result) = 0;
    virtual network_result<no_result> start_query(string_view query, execution_state& result) = 0;
    virtual network_result<no_result> prepare_statement(
        string_view statement,
        er_statement& stmt
    ) = 0;
    virtual network_result<row_view> read_one_row(execution_state& st) = 0;
    virtual network_result<rows_view> read_some_rows(execution_state& st) = 0;
    virtual network_result<no_result> quit() = 0;
    virtual network_result<no_result> close() = 0;
    virtual void sync_close() noexcept = 0;  // used by fixture cleanup functions
    virtual er_network_variant& variant() const = 0;
};

using er_connection_ptr = std::unique_ptr<er_connection>;

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
