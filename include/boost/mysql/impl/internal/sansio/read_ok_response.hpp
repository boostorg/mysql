//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_OK_RESPONSE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_OK_RESPONSE_HPP

#include <boost/mysql/diagnostics.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/sansio_algorithm.hpp>

namespace boost {
namespace mysql {
namespace detail {

class read_ok_response_algo : public sansio_algorithm
{
    diagnostics* diag_;
    std::uint8_t seqnum_{0};
    bool started_{false};

public:
    read_ok_response_algo(connection_state_data& st, diagnostics* diag) noexcept
        : sansio_algorithm(st), diag_(diag)
    {
    }

    diagnostics& diag() { return *diag_; }
    std::uint8_t& sequence_number() { return seqnum_; }

    next_action resume(error_code ec)
    {
        if (!started_)
        {
            started_ = true;

            // Issue a read
            return read(seqnum_);
        }
        else
        {
            // Process the OK packet
            if (ec)
                return ec;
            return st_->deserialize_ok(*diag_);
        }
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
