//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_RESULTSET_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_UTILS_INCLUDE_ER_RESULTSET_HPP

#include "network_result.hpp"
#include <boost/mysql/row.hpp>
#include <boost/mysql/metadata.hpp>
#include <memory>

namespace boost {
namespace mysql {
namespace test {

class er_resultset
{
public:
    virtual ~er_resultset() {}
    virtual network_result<bool> read_one(row&) = 0;
    virtual network_result<std::vector<row>> read_many(std::size_t count) = 0;
    virtual network_result<std::vector<row>> read_all() = 0;
    virtual bool valid() const = 0;
    virtual bool complete() const = 0;
    virtual const std::vector<metadata>& fields() const = 0;
    virtual std::uint64_t affected_rows() const = 0;
    virtual std::uint64_t last_insert_id() const = 0;
    virtual unsigned warning_count() const = 0;
    virtual boost::string_view info() const = 0;
};

using er_resultset_ptr = std::unique_ptr<er_resultset>;

} // test
} // mysql
} // boost


#endif