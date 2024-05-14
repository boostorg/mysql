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

class read_set_character_set_response_algo
{
    int resume_point_{0};
    diagnostics* diag_;
    character_set charset_;
    std::uint8_t seqnum_{0};

public:
    read_set_character_set_response_algo(diagnostics* diag, character_set charset)
        : diag_(diag), charset_(charset)
    {
    }
    character_set charset() const { return charset_; }
    diagnostics& diag() { return *diag_; }
    std::uint8_t& sequence_number() { return seqnum_; }

    next_action resume(connection_state_data& st, error_code ec)
    {
        // SET NAMES never returns rows. Using execute requires us to allocate
        // a results object, which we can avoid by simply sending the query and reading the OK response.
        switch (resume_point_)
        {
        case 0:

            // Read the response
            BOOST_MYSQL_YIELD(resume_point_, 1, st.read(seqnum_))

            // Verify it's what we expected
            ec = st.deserialize_ok(*diag_);
            if (ec)
                return ec;

            // If we were successful, update the character set
            st.current_charset = charset_;
        }

        return next_action();
    }
};

class set_character_set_algo
{
    int resume_point_{0};
    read_set_character_set_response_algo read_response_st_;

    next_action compose_request(connection_state_data& st)
    {
        auto q = compose_set_names(read_response_st_.charset());
        if (q.has_error())
            return q.error();
        return st.write(query_command{q.value()}, read_response_st_.sequence_number());
    }

public:
    set_character_set_algo(set_character_set_algo_params params) noexcept
        : read_response_st_(params.diag, params.charset)
    {
    }

    next_action resume(connection_state_data& st, error_code ec)
    {
        if (ec)
            return ec;

        next_action act;

        // SET NAMES never returns rows. Using execute requires us to allocate
        // a results object, which we can avoid by simply sending the query and reading the OK response.
        switch (resume_point_)
        {
        case 0:

            // Setup
            read_response_st_.diag().clear();

            // Send the execution request
            BOOST_MYSQL_YIELD(resume_point_, 1, compose_request(st))

            // Read the response
            while (!(act = read_response_st_.resume(st, ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 2, act)
            return act;
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
