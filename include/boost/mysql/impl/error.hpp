//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ERROR_HPP
#define BOOST_MYSQL_IMPL_ERROR_HPP

#pragma once

#include <boost/mysql/error.hpp>
#include <boost/mysql/impl/error_descriptions.hpp>
#include <boost/system/system_error.hpp>
#include <boost/config.hpp>
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

#ifdef BOOST_NO_CXX17_INLINE_VARIABLES

inline mysql_error_category_t& get_mysql_error_category() noexcept
{
    static mysql_error_category_t res;
    return res;
}

#else

inline mysql_error_category_t mysql_error_category;

inline mysql_error_category_t& get_mysql_error_category() noexcept
{
    return mysql_error_category;
}

#endif

inline void check_error_code(const error_code& code, const error_info& info)
{
    if (code)
    {
        throw boost::system::system_error(code, info.message());
    }
}

inline void clear_errors(error_code& err, error_info& info) noexcept
{
    err.clear();
    info.clear();
}

// Helper to implement sync with exceptions functions
struct error_block
{
    error_code err;
    error_info info;

    void check() { detail::check_error_code(err, info); }
};

} // detail
} // mysql
} // boost

inline std::ostream& boost::mysql::operator<<(
    std::ostream& os,
    errc value
)
{
    return os << detail::error_to_string(value);
}

inline boost::mysql::error_code boost::mysql::make_error_code(
    errc error
)
{
    return boost::system::error_code(static_cast<int>(error), detail::get_mysql_error_category());
}


#endif
