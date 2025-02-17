//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_QUIT_CONNECTION_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_QUIT_CONNECTION_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

class quit_connection_algo
{
    int resume_point_{0};
    std::uint8_t sequence_number_{0};

public:
    quit_connection_algo(quit_connection_algo_params) noexcept {}

    next_action resume(connection_state_data& st, diagnostics&, error_code ec)
    {
        switch (resume_point_)
        {
        case 0:

            // Send quit message
            BOOST_MYSQL_YIELD(resume_point_, 1, st.write(quit_command(), sequence_number_))

            // Mark the session as finished, regardless of the write result
            st.is_connected = false;

            // If write resulted in an error, return
            if (ec)
                return ec;

            // Attempt TLS shutdown. MySQL usually just closes the socket, instead of
            // sending the close_notify message required by the shutdown, so we ignore this error.
            if (st.ssl == ssl_state::active)
            {
                BOOST_MYSQL_YIELD(resume_point_, 2, next_action::ssl_shutdown())
                st.ssl = ssl_state::inactive;
            }

            // Done
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_QUIT_CONNECTION_HPP_ */
