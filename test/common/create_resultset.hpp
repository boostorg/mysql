//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATE_RESULTSET_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATE_RESULTSET_HPP

#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/resultset_base.hpp>

namespace boost {
namespace mysql {
namespace test {

template <class ResultsetType = resultset_base>
inline ResultsetType create_resultset(
    detail::resultset_encoding enc,
    const std::vector<detail::protocol_field_type>& types,
    std::uint8_t seqnum = 0
)
{
    ResultsetType res;
    res.reset(&res, enc);  // channel should just be != nullptr
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
