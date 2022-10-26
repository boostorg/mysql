//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_CONNECTION_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_CONNECTION_HPP

#include <boost/mysql/error.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/rows_view.hpp>

#include <memory>

#include "er_endpoint.hpp"
#include "er_resultset.hpp"
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
    virtual bool valid() const = 0;
    virtual bool uses_ssl() const = 0;
    virtual bool is_open() const = 0;
    virtual network_result<no_result> physical_connect(er_endpoint) = 0;
    virtual network_result<no_result> connect(er_endpoint, const handshake_params&) = 0;
    virtual network_result<no_result> handshake(const handshake_params&) = 0;
    virtual network_result<no_result> query(boost::string_view query, er_resultset& result) = 0;
    virtual network_result<no_result> prepare_statement(
        boost::string_view statement,
        er_statement& stmt
    ) = 0;
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
