//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_DIAGNOSTICS_HPP
#define BOOST_MYSQL_IMPL_DIAGNOSTICS_HPP

#include <boost/mysql/string_view.hpp>
#pragma once

#include <boost/mysql/diagnostics.hpp>

struct boost::mysql::detail::diagnostics_access
{
    static void assign(diagnostics& obj, string_view from, bool is_server = true)
    {
        obj.msg_.assign(from.begin(), from.end());
        obj.is_server_ = is_server;
    }
};

#endif
