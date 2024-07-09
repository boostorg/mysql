//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PING_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PING_HPP

#include <boost/mysql/detail/algo_params.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

class read_ping_response_algo
{
    int resume_point_{0};
    diagnostics* diag_;
    std::uint8_t seqnum_{0};

public:
    read_ping_response_algo(diagnostics* diag, std::uint8_t seqnum) noexcept : diag_(diag), seqnum_(seqnum) {}

    next_action resume(connection_state_data& st, error_code ec)
    {
        switch (resume_point_)
        {
        case 0:

            // Issue a read
            BOOST_MYSQL_YIELD(resume_point_, 1, st.read(seqnum_))
            if (ec)
                return ec;

            // Process the OK packet
            return st.deserialize_ok(*diag_);
        }
        return next_action();
    }
};

inline run_pipeline_algo_params setup_ping_pipeline(connection_state_data& st, ping_algo_params params)
{
    st.write_buffer.clear();
    auto seqnum = serialize_top_level(ping_command{}, st.write_buffer);
    st.shared_pipeline_stages[0] = {pipeline_stage_kind::ping, seqnum, {}};
    return {
        params.diag,
        st.write_buffer,
        {st.shared_pipeline_stages.data(), 1},
        nullptr
    };
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
