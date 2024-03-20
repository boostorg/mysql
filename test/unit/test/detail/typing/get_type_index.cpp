//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/typing/get_type_index.hpp>

#include <tuple>

#include "test_unit/row_identity.hpp"

using namespace boost::mysql::test;
using boost::mysql::detail::get_type_index;
using boost::mysql::detail::index_not_found;

namespace {

using r1 = std::tuple<int>;
using r2 = std::tuple<float>;
using r3 = std::tuple<double>;
using ri1 = row_identity<r1>;
using ri2 = row_identity<r2>;
using ri3 = row_identity<r3>;

// Unique types
static_assert(get_type_index<r1, r1, r2, r3>() == 0u, "");
static_assert(get_type_index<r2, r1, r2, r3>() == 1u, "");
static_assert(get_type_index<r3, r1, r2, r3>() == 2u, "");

// Unique types having a different underlying_row type
static_assert(get_type_index<r1, ri1, ri2, ri3>() == 0u, "");
static_assert(get_type_index<r2, ri1, ri2, ri3>() == 1u, "");
static_assert(get_type_index<r3, ri1, ri2, ri3>() == 2u, "");
static_assert(get_type_index<r2, ri1, ri2, r3>() == 1u, "");  // mixes are okay

// Single, repeated type
static_assert(get_type_index<r1, r1, r1, r1>() == 0u, "");
static_assert(get_type_index<r1, ri1, ri1, ri1>() == 0u, "");
static_assert(get_type_index<r1, r1, ri1, ri1>() == 0u, "");
static_assert(get_type_index<r1, ri1, r1, r1>() == 0u, "");

// Multiple, repeated types
static_assert(get_type_index<r1, r1, r2, r1, r3, r1, r2, r3, r1>() == 0u, "");
static_assert(get_type_index<r2, r1, r2, r1, r3, r1, r2, r3, r1>() == 1u, "");
static_assert(get_type_index<r3, r1, r2, r1, r3, r1, r2, r3, r1>() == 2u, "");
static_assert(get_type_index<r1, ri1, r2, r1, r3, r1, r2, r3, r1>() == 0u, "");

// Single type
static_assert(get_type_index<r1, r1>() == 0u, "");
static_assert(get_type_index<r1, ri1>() == 0u, "");

// Not found
static_assert(get_type_index<r1>() == index_not_found, "");
static_assert(get_type_index<r1, r2>() == index_not_found, "");
static_assert(get_type_index<r1, r2, r2, r3, r2>() == index_not_found, "");
static_assert(get_type_index<r1, ri2, r2, r3, r2>() == index_not_found, "");

}  // namespace
