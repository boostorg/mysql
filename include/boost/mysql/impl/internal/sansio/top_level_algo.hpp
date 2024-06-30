//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_TOP_LEVEL_ALGO_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_TOP_LEVEL_ALGO_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

#include <boost/core/span.hpp>

#include <cstddef>
#include <cstdint>

#ifdef BOOST_USE_VALGRIND
#include <valgrind/memcheck.h>
#endif

namespace boost {
namespace mysql {
namespace detail {

// Valgrind
#ifdef BOOST_USE_VALGRIND
inline void valgrind_make_mem_defined(const void* data, std::size_t size)
{
    VALGRIND_MAKE_MEM_DEFINED(data, size);
}
#else
inline void valgrind_make_mem_defined(const void*, std::size_t) noexcept {}
#endif

// InnerAlgo should have
//   Constructible from the args forwarded by the ctor.
//   next_action resume(connection_state_data&, error_code);
//   AlgoParams::result_type result(const connection_state_data&) const; // if AlgoParams::result_type != void
template <class InnerAlgo>
class top_level_algo
{
    int resume_point_{0};
    connection_state_data* st_;
    InnerAlgo algo_;
    span<const std::uint8_t> bytes_to_write_;

public:
    template <class... Args>
    top_level_algo(connection_state_data& st, Args&&... args) : st_(&st), algo_(std::forward<Args>(args)...)
    {
    }

    const InnerAlgo& inner_algo() const { return algo_; }

    next_action resume(error_code ec, std::size_t bytes_transferred)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            // Run until completion
            while (true)
            {
                // Run the op
                act = algo_.resume(*st_, ec);

                // Check next action
                if (act.is_done())
                {
                    return act;
                }
                else if (act.type() == next_action_type::read)
                {
                    // Read until a complete message is received
                    // (may be zero times if cached)
                    while (!st_->reader.done() && !ec)
                    {
                        ec = st_->reader.prepare_buffer();
                        if (ec)
                            break;
                        BOOST_MYSQL_YIELD(
                            resume_point_,
                            1,
                            next_action::read({st_->reader.buffer(), st_->ssl_active()})
                        )
                        valgrind_make_mem_defined(st_->reader.buffer().data(), bytes_transferred);
                        st_->reader.resume(bytes_transferred);
                    }

                    // Check for errors
                    if (!ec)
                        ec = st_->reader.error();

                    // We've got a message, continue
                }
                else if (act.type() == next_action_type::write)
                {
                    // Write until a complete message was written
                    bytes_to_write_ = act.write_args().buffer;

                    // Check buffer size. We should check this before
                    // resizing the buffer, but requires non-trivial changes.
                    // For now, this yields the right user-facing behavior.
                    // https://github.com/boostorg/mysql/issues/297
                    // https://github.com/boostorg/mysql/issues/279
                    if (bytes_to_write_.size() > st_->max_buffer_size())
                    {
                        ec = client_errc::max_buffer_size_exceeded;
                        continue;
                    }

                    while (!bytes_to_write_.empty() && !ec)
                    {
                        BOOST_MYSQL_YIELD(
                            resume_point_,
                            2,
                            next_action::write({bytes_to_write_, st_->ssl_active()})
                        )
                        bytes_to_write_ = bytes_to_write_.subspan(bytes_transferred);
                    }

                    // We fully wrote a message, continue
                }
                else
                {
                    // Other ops always require I/O
                    BOOST_MYSQL_YIELD(resume_point_, 3, act)
                }
            }
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
