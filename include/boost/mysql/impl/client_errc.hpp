//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CLIENT_ERRC_HPP
#define BOOST_MYSQL_IMPL_CLIENT_ERRC_HPP

#pragma once

#include <boost/mysql/client_errc.hpp>

#include <ostream>

namespace boost {
namespace system {

template <>
struct is_error_code_enum<::boost::mysql::client_errc>
{
    static constexpr bool value = true;
};

}  // namespace system

namespace mysql {
namespace detail {

inline const char* error_to_string(client_errc error) noexcept
{
    switch (error)
    {
    case client_errc::incomplete_message: return "An incomplete message was received from the server";
    case client_errc::extra_bytes: return "Unexpected extra bytes at the end of a message were received";
    case client_errc::sequence_number_mismatch: return "Mismatched sequence numbers";
    case client_errc::server_unsupported:
        return "The server does not support the minimum required capabilities to establish the "
               "connection";
    case client_errc::protocol_value_error:
        return "An unexpected value was found in a server-received message";
    case client_errc::unknown_auth_plugin:
        return "The user employs an authentication plugin not known to this library";
    case client_errc::auth_plugin_requires_ssl:
        return "The authentication plugin requires the connection to use SSL";
    case client_errc::wrong_num_params:
        return "The number of parameters passed to the prepared statement does not match the "
               "number of actual parameters";
    default: return "<unknown MySQL client error>";
    }
}

class client_category_t : public boost::system::error_category
{
public:
    const char* name() const noexcept final override { return "mysql.client"; }
    std::string message(int ev) const final override { return error_to_string(static_cast<client_errc>(ev)); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

const boost::system::error_category& boost::mysql::get_client_category() noexcept
{
    static detail::client_category_t res;
    return res;
}

boost::mysql::error_code boost::mysql::make_error_code(client_errc error)
{
    return error_code(static_cast<int>(error), get_client_category());
}

std::ostream& boost::mysql::operator<<(std::ostream& os, client_errc v)
{
    return os << detail::error_to_string(v);
}

#endif
