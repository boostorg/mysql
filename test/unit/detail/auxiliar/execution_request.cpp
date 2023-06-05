//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_concepts.hpp>

#include <string>
#ifdef __cpp_lib_string_view
#include <string_view>
#endif

using namespace boost::mysql;
using detail::is_execution_request;

// -----
// strings
// -----
static_assert(is_execution_request<std::string>::value, "");
static_assert(is_execution_request<const std::string&>::value, "");
static_assert(is_execution_request<std::string&&>::value, "");
static_assert(is_execution_request<std::string&>::value, "");

static_assert(is_execution_request<string_view>::value, "");
static_assert(is_execution_request<const string_view&>::value, "");
static_assert(is_execution_request<string_view&&>::value, "");

#ifdef __cpp_lib_string_view
static_assert(is_execution_request<std::string_view>::value, "");
static_assert(is_execution_request<const std::string_view&>::value, "");
static_assert(is_execution_request<std::string_view&&>::value, "");
#endif

static_assert(is_execution_request<const char*>::value, "");

static_assert(is_execution_request<const char[14]>::value, "");
static_assert(is_execution_request<const char (&)[14]>::value, "");

// -----
// tuple statements
// -----
using tup_type = std::tuple<int, const char*, std::nullptr_t>;
static_assert(is_execution_request<bound_statement_tuple<tup_type>>::value, "");
static_assert(is_execution_request<const bound_statement_tuple<tup_type>&>::value, "");
static_assert(is_execution_request<bound_statement_tuple<tup_type>&>::value, "");
static_assert(is_execution_request<bound_statement_tuple<tup_type>&&>::value, "");

static_assert(is_execution_request<bound_statement_tuple<std::tuple<>>>::value, "");

// tuples not accepted
static_assert(!is_execution_request<tup_type>::value, "");
static_assert(!is_execution_request<const tup_type&>::value, "");
static_assert(!is_execution_request<tup_type&>::value, "");
static_assert(!is_execution_request<tup_type&&>::value, "");

// -----
// iterator statements
// -----
static_assert(is_execution_request<bound_statement_iterator_range<field_view*>>::value, "");
static_assert(is_execution_request<const bound_statement_iterator_range<field_view*>&>::value, "");
static_assert(is_execution_request<bound_statement_iterator_range<field_view*>&>::value, "");
static_assert(is_execution_request<bound_statement_iterator_range<field_view*>&&>::value, "");

// iterators not accepted
static_assert(!is_execution_request<field_view*>::value, "");

// Other stuff not accepted
static_assert(!is_execution_request<field_view>::value, "");
static_assert(!is_execution_request<int>::value, "");
static_assert(!is_execution_request<std::nullptr_t>::value, "");
