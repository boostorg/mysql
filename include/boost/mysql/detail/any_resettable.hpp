//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ERASE_RESETTABLE_HPP
#define BOOST_MYSQL_DETAIL_ERASE_RESETTABLE_HPP

#include <memory>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

class any_resettable
{
    using destructor_fn = void (*)(void*);
    using reset_fn = void (*)(void*);

    std::unique_ptr<void, destructor_fn> impl_;
    reset_fn reset_;

    template <class T>
    static void call_destructor(void* obj)
    {
        delete static_cast<T*>(obj);
    }

    template <class T>
    static void call_reset(void* obj)
    {
        static_cast<T*>(obj)->reset();
    }

    explicit any_resettable(void* obj, destructor_fn destr, reset_fn rst) noexcept
        : impl_(obj, destr), reset_(rst)
    {
    }

public:
    template <class T, class... Args>
    static any_resettable make(Args&&... args)
    {
        return any_resettable(new T{std::forward<Args>(args)...}, &call_destructor<T>, &call_reset<T>);
    }

    void* get() const { return impl_.get(); }
    void reset() { reset_(get()); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
