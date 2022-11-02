//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_BUFFER_PARAMS_HPP
#define BOOST_MYSQL_BUFFER_PARAMS_HPP

#include <cstddef>

namespace boost {
namespace mysql {

/**
 * \brief Buffer configuration parameters for a connection.
 * \details See [link mysql.connparams the section on handshake parameters] for more info.
 */
class buffer_params
{
    std::size_t initial_read_buffer_size_;

public:
    /**
     * \brief Initializing constructor.
     * \param init_read_buffer_size Initial size of the read buffer. A bigger read buffer
     * can increase the number of rows returned by [refmem resultset read_some].
     */
    constexpr buffer_params(std::size_t init_read_buffer_size = 0) noexcept
        : initial_read_buffer_size_(init_read_buffer_size)
    {
    }

    /// Gets the initial size of the read buffer.
    constexpr std::size_t initial_read_buffer_size() const noexcept
    {
        return initial_read_buffer_size_;
    }

    /// Sets the initial size of the read buffer.
    void set_initial_read_buffer_size(std::size_t v) noexcept { initial_read_buffer_size_ = v; }
};

}  // namespace mysql
}  // namespace boost

#endif
