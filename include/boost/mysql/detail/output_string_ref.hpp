//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_OUTPUT_STRING_REF_HPP
#define BOOST_MYSQL_DETAIL_OUTPUT_STRING_REF_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/config.hpp>

#include <cstddef>
#include <cstring>

#ifdef BOOST_MYSQL_HAS_CONCEPTS
#include <concepts>
#endif

namespace boost {
namespace mysql {
namespace detail {

#ifdef BOOST_MYSQL_HAS_CONCEPTS

template <class T>
concept output_string = requires(T& t) {
    {
        t[0]
    } -> std::same_as<char&>;
    {
        t.size()
    } -> std::convertible_to<std::size_t>;
    t.clear();
    t.resize(std::size_t{});
};

#define BOOST_MYSQL_OUTPUT_STRING ::boost::mysql::detail::output_string

#else

#define BOOST_MYSQL_OUTPUT_STRING class

#endif

class output_string_ref
{
    using append_fn_t = void (*)(void*, const char*, std::size_t);

    append_fn_t append_fn_;
    void* container_;

    template <class T>
    static void do_append(void* container, const char* data, std::size_t size)
    {
        auto& obj = *static_cast<T*>(container);
        std::size_t prev_size = obj.size();
        obj.resize(prev_size + size);
        std::memcpy(&obj[0] + prev_size, data, size);  // data() is non-const in C++ < 17
    }

public:
    output_string_ref(append_fn_t append_fn, void* container) noexcept
        : append_fn_(append_fn), container_(container)
    {
    }

    template <class T>
    static output_string_ref create(T& obj) noexcept
    {
        return output_string_ref(&do_append<T>, &obj);
    }

    void append(string_view data)
    {
        if (data.size() > 0u)
            append_fn_(container_, data.data(), data.size());
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
