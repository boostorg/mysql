//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_WITH_DIAGNOSTICS_HPP
#define BOOST_MYSQL_WITH_DIAGNOSTICS_HPP

#include <boost/mysql/detail/access.hpp>

#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {

// TODO: document
// Transforms handler signature from void(error_code, Args...) to void(std::exception_ptr, Args...)
// On error, the std::exception_ptr will point to a mysql::error_with_diagnostics, holding
// extra server-supplied diagnostics.
// Can only be used with Boost.MySQL initiation functions, as it uses internal knowledge to grab diagnostics.
// Can be used to wrap other completion tokens, e.g. with_diagnostics(deferred), with_diagnostics(yield).
// Can't be wrapped: consign(with_diagnostics(X)) is not supported.
template <class CompletionToken>
class with_diagnostics_t
{
    CompletionToken impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    template <class T>
    constexpr explicit with_diagnostics_t(T&& token) : impl_(std::forward<T>(token))
    {
    }
};

template <class CompletionToken>
constexpr with_diagnostics_t<typename std::decay<CompletionToken>::type> with_diagnostics(
    CompletionToken&& token
)
{
    return with_diagnostics_t<typename std::decay<CompletionToken>::type>(std::forward<CompletionToken>(token)
    );
}

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/with_diagnostics.hpp>

#endif
