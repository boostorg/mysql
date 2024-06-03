//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_OPERATORS_PIPELINE_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_OPERATORS_PIPELINE_HPP

#include <boost/mysql/detail/pipeline.hpp>

#include <iosfwd>

namespace boost {
namespace mysql {
namespace detail {

// pipeline_stage_kind
std::ostream& operator<<(std::ostream& os, pipeline_stage_kind v);

// pipeline_request_stage
bool operator==(const pipeline_request_stage& lhs, const pipeline_request_stage& rhs);
std::ostream& operator<<(std::ostream& os, pipeline_request_stage v);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
