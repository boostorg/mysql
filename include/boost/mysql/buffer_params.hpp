//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONN_INIT_PARAMS_HPP
#define BOOST_MYSQL_CONN_INIT_PARAMS_HPP

#include <cstddef>

namespace boost {
namespace mysql {

class buffer_params
{
    std::size_t initial_read_buffer_size_;

public:
    constexpr buffer_params(std::size_t init_read_buffer_size = 0) noexcept
        : initial_read_buffer_size_(init_read_buffer_size)
    {
    }

    constexpr std::size_t initial_read_buffer_size() const noexcept
    {
        return initial_read_buffer_size_;
    }
};

}  // namespace mysql
}  // namespace boost

#endif
