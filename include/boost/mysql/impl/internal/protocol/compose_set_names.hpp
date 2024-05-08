//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_COMPOSE_SET_NAMES_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_PROTOCOL_COMPOSE_SET_NAMES_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/format_sql.hpp>

#include <boost/function/function_base.hpp>
#include <boost/system/result.hpp>

#include <string>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

// Securely compose a SET NAMES statement
inline system::result<std::string> compose_set_names(character_set charset)
{
    // The character set should have a non-empty name
    BOOST_ASSERT(!charset.name.empty());

    // For security, if the character set has non-ascii characters in it name, reject it.
    format_context ctx(format_options{ascii_charset, true});
    ctx.append_raw("SET NAMES ").append_value(charset.name);
    return std::move(ctx).get();
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
