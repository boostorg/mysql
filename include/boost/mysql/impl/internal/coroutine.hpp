//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_COROUTINE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_COROUTINE_HPP

// asio::coroutine uses __COUNTER__ internally, which can trigger
// ODR violations if we use them in header-only code. These manifest as
// extremely hard-to-debug bugs only present in release builds

// Coroutine state is represented as an integer (resume_point_var).
// Every yield gets assigned a unique value (resume_point_id).
// Yielding sets the next resume point, returns, and sets a case label for re-entering.
// Coroutines need to switch on resume_point_var to re-enter.

// Enclosing this in a scope allows placing the macro inside a brace-less for/while loop
// The empty scope after the case label is required because labels can't be at the end of a compound statement
#define BOOST_MYSQL_YIELD(resume_point_var, resume_point_id, ...) \
    {                                                             \
        resume_point_var = resume_point_id;                       \
        return __VA_ARGS__;                                       \
    case resume_point_id:                                         \
    {                                                             \
    }                                                             \
    }

#define BOOST_MYSQL_YIELD_VOID(resume_point_var, resume_point_id) \
    BOOST_MYSQL_YIELD(resume_point_var, resume_point_id, static_cast<void>(0))

#endif
