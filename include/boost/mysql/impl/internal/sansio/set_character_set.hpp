//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_SET_CHARACTER_SET_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_SET_CHARACTER_SET_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/compose_set_names.hpp>
#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

#include <boost/system/result.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

class set_character_set_algo
{
    connection_state_data* st_;
    int resume_point_{0};
    diagnostics* diag_;
    character_set charset_;
    std::uint8_t seqnum_{0};

    next_action compose_request()
    {
        auto q = compose_set_names(charset_);
        if (q.has_error())
            return q.error();
        return st_->write(query_command{q.value()}, seqnum_);
    }

public:
    set_character_set_algo(connection_state_data& st, set_character_set_algo_params params) noexcept
        : st_(&st), diag_(params.diag), charset_(params.charset)
    {
    }

    connection_state_data& conn_state() { return *st_; }

    next_action resume(error_code ec)
    {
        if (ec)
            return ec;

        // SET NAMES never returns rows. Using execute requires us to allocate
        // a results object, which we can avoid by simply sending the query and reading the OK response.
        switch (resume_point_)
        {
        case 0:

            // Setup
            diag_->clear();

            // Send the execution request
            BOOST_MYSQL_YIELD(resume_point_, 1, compose_request())

            // Read the response
            BOOST_MYSQL_YIELD(resume_point_, 2, st_->read(seqnum_))

            // Verify it's what we expected
            ec = st_->deserialize_ok(*diag_);
            if (ec)
                return ec;

            // If we were successful, update the character set
            st_->current_charset = charset_;
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
