//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ERROR_HPP
#define BOOST_MYSQL_IMPL_ERROR_HPP

#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <algorithm>

namespace boost {
namespace system {

template <>
struct is_error_code_enum<mysql::errc>
{
    static constexpr bool value = true;
};

} // system

namespace mysql {
namespace detail {

struct error_entry
{
    errc value;
    const char* message;
};

constexpr error_entry all_errors [] = {
    { errc::ok, "no error" },
    { errc::incomplete_message, "The message read was incomplete (not enough bytes to fully decode it)" },
    { errc::extra_bytes, "Extra bytes at the end of the message" },
    { errc::sequence_number_mismatch, "Mismatched sequence numbers" },
    { errc::server_unsupported, "The server does not implement the minimum features to be supported" },
    { errc::protocol_value_error, "A field in a message had an unexpected value" },
    { errc::unknown_auth_plugin, "The user employs an authentication plugin unknown to the client" },
    { errc::wrong_num_params, "The provided parameter count does not match the prepared statement parameter count" },

#include "boost/mysql/impl/server_error_descriptions.hpp"

};

inline const char* error_to_string(errc error) noexcept
{
    auto it = std::find_if(
        std::begin(all_errors),
        std::end(all_errors),
        [error] (error_entry ent) { return error == ent.value; }
    );
    return it == std::end(all_errors) ? "<unknown error>" : it->message;
}

class mysql_error_category_t : public boost::system::error_category
{
public:
    const char* name() const noexcept final override { return "mysql"; }
    std::string message(int ev) const final override
    {
        return error_to_string(static_cast<errc>(ev));
    }
};
inline mysql_error_category_t mysql_error_category;

inline boost::system::error_code make_error_code(errc error)
{
    return boost::system::error_code(static_cast<int>(error), mysql_error_category);
}

inline void check_error_code(const error_code& code, const error_info& info)
{
    if (code)
    {
        throw boost::system::system_error(code, info.message());
    }
}

inline void conditional_clear(error_info* info) noexcept
{
    if (info)
    {
        info->clear();
    }
}

inline void conditional_assign(error_info* to, error_info&& from)
{
    if (to)
    {
        *to = std::move(from);
    }
}

inline void clear_errors(error_code& err, error_info& info) noexcept
{
    err.clear();
    info.clear();
}

} // detail
} // mysql
} // boost


#endif
