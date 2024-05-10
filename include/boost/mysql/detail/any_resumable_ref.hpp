//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ANY_RESUMABLE_REF_HPP
#define BOOST_MYSQL_DETAIL_ANY_RESUMABLE_REF_HPP

#include <boost/mysql/detail/next_action.hpp>

#include <cstddef>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

class any_resumable_ref
{
    template <class T>
    static next_action do_resume(void* self, error_code ec, std::size_t bytes_transferred)
    {
        return static_cast<T*>(self)->resume(ec, bytes_transferred);
    }

    using fn_t = next_action (*)(void*, error_code, std::size_t);

    void* algo_{};
    fn_t fn_{};

public:
    template <class T, class = typename std::enable_if<!std::is_same<T, any_resumable_ref>::value>::type>
    explicit any_resumable_ref(T& op) noexcept : algo_(&op), fn_(&do_resume<T>)
    {
    }

    next_action resume(error_code ec, std::size_t bytes_transferred)
    {
        return fn_(algo_, ec, bytes_transferred);
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
