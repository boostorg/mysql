//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_STATEMENT_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_STATEMENT_HPP

#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/statement_base.hpp>

#include <forward_list>
#include <memory>
#include <vector>

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
    virtual network_result<no_result> execute_tuple2(
        field_view fv1,
        field_view fv2,
        resultset& result
    ) = 0;
    virtual network_result<no_result> start_execution_tuple2(
        field_view fv1,
        field_view fv2,
        execution_state& st
    ) = 0;
    virtual network_result<no_result> start_execution_it(
        value_list_it params_first,
        value_list_it params_last,
        execution_state& st
    ) = 0;
    virtual network_result<no_result> close() = 0;
};

using er_statement_ptr = std::unique_ptr<er_statement>;

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
