//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_RESULTSET_VIEW_HPP
#define BOOST_MYSQL_IMPL_RESULTSET_VIEW_HPP

#pragma once

#include <boost/mysql/resultset_view.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>

struct boost::mysql::detail::resultset_view_access
{
    static resultset_view construct(const detail::results_impl& st, std::size_t index)
    {
        return resultset_view(st, index);
    }
};

#endif
