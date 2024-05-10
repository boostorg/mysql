//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_OK_RESPONSE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_OK_RESPONSE_HPP

#include <boost/mysql/diagnostics.hpp>

#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

class read_ok_response_algo
{
    connection_state_data* st_;
    int resume_point_{0};
    diagnostics* diag_;
    std::uint8_t seqnum_{0};

public:
    read_ok_response_algo(connection_state_data& st, diagnostics* diag) noexcept : st_(&st), diag_(diag) {}

    connection_state_data& conn_state() { return *st_; }
    diagnostics& diag() { return *diag_; }
    std::uint8_t& sequence_number() { return seqnum_; }

    next_action resume(error_code ec)
    {
        switch (resume_point_)
        {
        case 0:

            // Issue a read
            BOOST_MYSQL_YIELD(resume_point_, 1, st_->read(seqnum_))
            if (ec)
                return ec;

            // Process the OK packet
            return st_->deserialize_ok(*diag_);
        }
        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
