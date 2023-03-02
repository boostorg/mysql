//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_EXECUTION_STATE_HPP
#define BOOST_MYSQL_IMPL_EXECUTION_STATE_HPP

#pragma once

#include <boost/mysql/execution_state.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>

struct boost::mysql::detail::execution_state_access
{
    static detail::execution_state_impl& get_impl(execution_state& st) noexcept { return st.impl_; }
};

#endif
