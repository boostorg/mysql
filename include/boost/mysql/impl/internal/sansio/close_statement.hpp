//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CLOSE_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_CLOSE_STATEMENT_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

class close_statement_algo
{
    int resume_point_{0};
    diagnostics* diag_;
    std::uint32_t stmt_id_;
    std::uint8_t close_seqnum_{0};
    std::uint8_t ping_seqnum_{0};

public:
    close_statement_algo(close_statement_algo_params params) noexcept
        : diag_(params.diag), stmt_id_(params.stmt_id)
    {
    }

    next_action resume(connection_state_data& st, error_code ec)
    {
        if (ec)
            return ec;

        switch (resume_point_)
        {
        case 0:

            // Clear diagnostics
            diag_->clear();

            // Compose the requests. We pipeline a ping with the close statement
            // to force the server send a response. Otherwise, the client ends up waiting
            // for the next TCP ACK, which takes some milliseconds to be sent
            // (see https://github.com/boostorg/mysql/issues/181)
            BOOST_MYSQL_YIELD(
                resume_point_,
                1,
                st.write(close_stmt_command{stmt_id_}, close_seqnum_, ping_command{}, ping_seqnum_)
            )

            // Read ping response
            BOOST_MYSQL_YIELD(resume_point_, 2, st.read(ping_seqnum_))

            // Process the OK packet
            return st.deserialize_ok(*diag_);
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_HPP_ */
