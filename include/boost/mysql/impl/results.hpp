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

struct boost::mysql::detail::results_access
{
    static execution_state_impl& get_state(results& result) noexcept { return result.st_; }
};

#endif
