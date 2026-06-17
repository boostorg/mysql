//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_LIGHTWEIGHT_ANY_HPP
#define BOOST_MYSQL_DETAIL_LIGHTWEIGHT_ANY_HPP

#include <memory>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

// Owning value that might point to any type.
// A lightweight version of std::any that requires no RTTI.

using lightweight_any = std::unique_ptr<void, void (*)(void*)>;

template <class T>
static void delete_lightweight_any(void* obj)
{
    delete static_cast<T*>(obj);
}

template <class T, class... Args>
lightweight_any make_lightweight_any(Args&&... args)
{
    return lightweight_any{new T{std::forward<Args>(args)...}, &delete_lightweight_any<T>};
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
