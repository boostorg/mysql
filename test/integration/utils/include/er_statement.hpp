//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_STATEMENT_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_STATEMENT_HPP

#include <memory>
#include "network_result.hpp"
#include "er_resultset.hpp"
#include <forward_list>
#include <vector>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/execute_params.hpp>

namespace boost {
namespace mysql {
namespace test {

using value_list_it = std::forward_list<field_view>::const_iterator;

class er_statement
{
public:
    virtual ~er_statement() {}
    virtual bool valid() const = 0;
    virtual unsigned id() const = 0;
    virtual unsigned num_params() const = 0;
    virtual network_result<er_resultset_ptr> execute_container(const std::vector<field_view>&) = 0;
    virtual network_result<er_resultset_ptr> execute_params(const execute_params<value_list_it>& params) = 0;
    virtual network_result<no_result> close() = 0;
};

using er_statement_ptr = std::unique_ptr<er_statement>;


} // test
} // mysql
} // boost


#endif
