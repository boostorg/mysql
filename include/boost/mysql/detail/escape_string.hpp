//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ESCAPE_STRING_HPP
#define BOOST_MYSQL_DETAIL_ESCAPE_STRING_HPP

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/output_string_ref.hpp>

namespace boost {
namespace mysql {

// Forward decls
struct character_set;

namespace detail {

BOOST_MYSQL_DECL
error_code escape_string(
    string_view input,
    const character_set& charset,
    bool backslash_escapes,
    char quote_char,
    output_string_ref output
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
