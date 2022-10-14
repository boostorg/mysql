//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_STATEMENT_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_STATEMENT_HPP

#include <boost/mysql/execute_params.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/statement_base.hpp>

#include <forward_list>
#include <memory>
#include <vector>

#include "er_resultset.hpp"
#include "network_result.hpp"

namespace boost {
namespace mysql {
namespace test {

using value_list_it = std::forward_list<field_view>::const_iterator;

class er_statement
{
public:
    virtual ~er_statement() {}
    virtual const statement_base& base() const = 0;
    virtual network_result<no_result> execute_collection(
        const std::vector<field_view>& params,
        er_resultset& result
    ) = 0;
    virtual network_result<no_result> execute_params(
        const execute_params<value_list_it>& params,
        er_resultset& result
    ) = 0;
    virtual network_result<no_result> close() = 0;
};

using er_statement_ptr = std::unique_ptr<er_statement>;

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
