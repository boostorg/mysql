//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/static_execution_state.hpp>
#include <boost/mysql/static_results.hpp>

#include <boost/mysql/detail/execution_processor/concepts.hpp>

#ifdef BOOST_MYSQL_HAS_CONCEPTS

using namespace boost::mysql;
using detail::execution_state_type;
using detail::results_type;

namespace {

using row1 = std::tuple<int, float>;
using row2 = std::tuple<double>;

// -----
// execution_state_type
// -----
static_assert(!execution_state_type<std::string>, "");
static_assert(!execution_state_type<int>, "");
static_assert(!execution_state_type<results>, "");
static_assert(!execution_state_type<static_results<row1>>, "");
static_assert(!execution_state_type<static_results<row1, row2>>, "");
static_assert(execution_state_type<execution_state>, "");
static_assert(execution_state_type<static_execution_state<row1>>, "");
static_assert(execution_state_type<static_execution_state<row1, row2>>, "");

// -----
// results_type
// -----
static_assert(!results_type<std::string>, "");
static_assert(!results_type<int>, "");
static_assert(!results_type<execution_state>, "");
static_assert(!results_type<static_execution_state<row1>>, "");
static_assert(!results_type<static_execution_state<row1, row2>>, "");
static_assert(results_type<results>, "");
static_assert(results_type<static_results<row1>>, "");
static_assert(results_type<static_results<row1, row2>>, "");

}  // namespace

#endif
