//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_MESSAGE_STRUCT_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_MESSAGE_STRUCT_HPP

#include <boost/mysql/detail/protocol/common_messages.hpp>

namespace boost {
namespace mysql {
namespace test {

inline detail::ok_packet create_ok_packet(
    std::uint64_t affected_rows = 0,
    std::uint64_t last_insert_id = 0,
    std::uint16_t status_flags = 0,
    std::uint16_t warnings = 0,
    string_view info = ""
)
{
    return detail::ok_packet{
        detail::int_lenenc{affected_rows},
        detail::int_lenenc{last_insert_id},
        status_flags,
        warnings,
        detail::string_lenenc{info},
    };
}

inline detail::err_packet create_err_packet(std::uint16_t code, string_view message = "")
{
    return detail::err_packet{
        code,
        detail::string_fixed<1>{},
        detail::string_fixed<5>{},
        detail::string_eof{message},
    };
}

inline detail::column_definition_packet create_coldef(
    detail::protocol_field_type type,
    string_view name = "mycol"
)
{
    return boost::mysql::detail::column_definition_packet{
        detail::string_lenenc("def"),
        detail::string_lenenc("mydb"),
        detail::string_lenenc("mytable"),
        detail::string_lenenc("mytable"),
        detail::string_lenenc(name),
        detail::string_lenenc(name),
        33,  // utf8_general_ci
        10,  // column_length
        type,
        0,  // flags
        0,  // decimals
    };
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
