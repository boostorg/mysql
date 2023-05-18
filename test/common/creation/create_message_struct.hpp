//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_MESSAGE_STRUCT_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_MESSAGE_STRUCT_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>

namespace boost {
namespace mysql {
namespace test {

class ok_builder
{
    detail::ok_packet pack_{};

    void flag(std::uint16_t f, bool value)
    {
        if (value)
            pack_.status_flags |= f;
        else
            pack_.status_flags &= ~f;
    }

public:
    ok_builder() = default;
    ok_builder& affected_rows(std::uint64_t v)
    {
        pack_.affected_rows.value = v;
        return *this;
    }
    ok_builder& last_insert_id(std::uint64_t v)
    {
        pack_.last_insert_id.value = v;
        return *this;
    }
    ok_builder& warnings(std::uint16_t v)
    {
        pack_.warnings = v;
        return *this;
    }
    ok_builder& more_results(bool v)
    {
        flag(detail::SERVER_MORE_RESULTS_EXISTS, v);
        return *this;
    }
    ok_builder& out_params(bool v)
    {
        flag(detail::SERVER_PS_OUT_PARAMS, v);
        return *this;
    }
    ok_builder& info(string_view v)
    {
        pack_.info.value = v;
        return *this;
    }
    detail::ok_packet build() { return pack_; }
};

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
