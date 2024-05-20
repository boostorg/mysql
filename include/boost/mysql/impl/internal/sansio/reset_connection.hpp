//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_RESET_CONNECTION_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_RESET_CONNECTION_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/algo_params.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/compose_set_names.hpp>
#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

class reset_connection_algo
{
    int resume_point_{0};
    diagnostics* diag_;
    std::uint8_t seqnum_{0};

public:
    reset_connection_algo(reset_connection_algo_params params) noexcept : diag_(params.diag) {}

    next_action resume(connection_state_data& st, error_code ec)
    {
        if (ec)
            return ec;

        switch (resume_point_)
        {
        case 0:

            // Clear diagnostics
            diag_->clear();

            // Send the request
            BOOST_MYSQL_YIELD(resume_point_, 1, st.write(reset_connection_command{}, seqnum_))

            // Read the reset response
            BOOST_MYSQL_YIELD(resume_point_, 2, st.read(seqnum_))

            // Verify it's what we expected
            ec = st.deserialize_ok(*diag_);
            if (!ec)
            {
                // Reset was successful. Resetting changes the connection's character set
                // to the server's default, which is an unknown value that doesn't have to match
                // what was specified in handshake. As a safety measure, clear the current charset
                st.current_charset = character_set{};
            }

            // Done
            return ec;
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP_ */
