//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_SERVER_ERRC_HPP
#define BOOST_MYSQL_IMPL_SERVER_ERRC_HPP

#pragma once

#include <boost/mysql/server_errc.hpp>

#include <boost/mysql/impl/server_errc_strings.hpp>

#include <ostream>

namespace boost {
namespace system {

template <>
struct is_error_code_enum<::boost::mysql::server_errc>
{
    static constexpr bool value = true;
};

}  // namespace system

namespace mysql {
namespace detail {

class server_category_t : public boost::system::error_category
{
public:
    const char* name() const noexcept final override { return "mysql.server"; }
    std::string message(int ev) const final override { return error_to_string(static_cast<server_errc>(ev)); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

const boost::system::error_category& boost::mysql::get_server_category() noexcept
{
    static detail::server_category_t res;
    return res;
}

boost::mysql::error_code boost::mysql::make_error_code(server_errc error)
{
    return error_code(static_cast<int>(error), get_server_category());
}

std::ostream& boost::mysql::operator<<(std::ostream& os, server_errc v)
{
    return os << detail::error_to_string(v);
}

#endif
