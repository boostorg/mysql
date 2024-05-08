//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PING_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PING_HPP

#include <boost/mysql/detail/algo_params.hpp>

#include <boost/mysql/impl/internal/sansio/next_action.hpp>
#include <boost/mysql/impl/internal/sansio/read_ok_response.hpp>
#include <boost/mysql/impl/internal/sansio/sansio_algorithm.hpp>

#include <boost/asio/coroutine.hpp>

namespace boost {
namespace mysql {
namespace detail {

class ping_algo : public sansio_algorithm, asio::coroutine
{
    read_ok_response_algo read_response_st_;

public:
    ping_algo(connection_state_data& st, ping_algo_params params) noexcept
        : sansio_algorithm(st), read_response_st_(st, params.diag)
    {
    }

    next_action resume(error_code ec)
    {
        next_action act;

        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Clear diagnostics
            read_response_st_.diag().clear();

            // Send the request
            BOOST_ASIO_CORO_YIELD return write(ping_command(), read_response_st_.sequence_number());
            if (ec)
                return ec;

            // Read the response
            while (!(act = read_response_st_.resume(ec)).is_done())
                BOOST_ASIO_CORO_YIELD return act;
            return act;
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
