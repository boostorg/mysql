//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_RESULTS_HPP
#define BOOST_MYSQL_IMPL_RESULTS_HPP

#pragma once

#include <boost/mysql/results.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>

struct boost::mysql::detail::results_access
{
    static results_impl& get_impl(results& result) noexcept { return result.impl_; }
};

#endif
