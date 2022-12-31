//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATE_EXECUTION_STATE_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATE_EXECUTION_STATE_HPP

#include <boost/mysql/execution_state.hpp>

#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

namespace boost {
namespace mysql {
namespace test {

inline execution_state create_execution_state(
    detail::resultset_encoding enc,
    const std::vector<detail::protocol_field_type>& types,
    std::uint8_t seqnum = 0
)
{
    execution_state res;
    res.reset(enc);
    boost::mysql::detail::column_definition_packet coldef;
    for (auto type : types)
    {
        coldef.type = type;
        coldef.flags = 0;
        coldef.decimals = 0;
        res.add_meta(coldef);
    }
    res.sequence_number() = seqnum;
    return res;
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
