//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_RESULTSET_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_RESULTSET_HPP

#include <boost/mysql/resultset_base.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>

#include <memory>

#include "network_result.hpp"

namespace boost {
namespace mysql {
namespace test {

class er_resultset
{
public:
    virtual ~er_resultset() {}
    virtual const resultset_base& base() const noexcept = 0;
    virtual network_result<row_view> read_one() = 0;
    virtual network_result<rows_view> read_some() = 0;
    virtual network_result<rows_view> read_all() = 0;
};

using er_resultset_ptr = std::unique_ptr<er_resultset>;

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif