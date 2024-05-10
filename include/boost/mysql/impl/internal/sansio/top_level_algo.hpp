//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_TOP_LEVEL_ALGO_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_TOP_LEVEL_ALGO_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

#include <cstddef>

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
//   Constructible from connection_state_data& and any other args forwarded by the ctor.
//   connection_state_data& conn_state();
//   next_action resume(error_code);
//   AlgoParams::result_type result() const; // if AlgoParams::result_type is not void
template <class InnerAlgo>
class top_level_algo
{
    int resume_point_{0};
    InnerAlgo algo_;

    connection_state_data& conn_state() noexcept { return algo_.conn_state(); }

public:
    template <class... Args>
    top_level_algo(connection_state_data& st_data, Args&&... args)
        : algo_(st_data, std::forward<Args>(args)...)
    {
    }

    const InnerAlgo& inner_algo() const noexcept { return algo_; }

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
                act = algo_.resume(ec);

                // Check next action
                if (act.is_done())
                {
                    return act;
                }
                else if (act.type() == next_action::type_t::read)
                {
                    // Read until a complete message is received
                    // (may be zero times if cached)
                    while (!conn_state().reader.done() && !ec)
                    {
                        conn_state().reader.prepare_buffer();
                        BOOST_MYSQL_YIELD(
                            resume_point_,
                            1,
                            next_action::read({conn_state().reader.buffer(), conn_state().ssl_active()})
                        )
                        valgrind_make_mem_defined(conn_state().reader.buffer().data(), bytes_transferred);
                        conn_state().reader.resume(bytes_transferred);
                    }

                    // Check for errors
                    if (!ec)
                        ec = conn_state().reader.error();

                    // We've got a message, continue
                }
                else if (act.type() == next_action::type_t::write)
                {
                    // Write until a complete message was written
                    while (!conn_state().writer.done() && !ec)
                    {
                        BOOST_MYSQL_YIELD(
                            resume_point_,
                            2,
                            next_action::write(
                                {conn_state().writer.current_chunk(), conn_state().ssl_active()}
                            )
                        )
                        conn_state().writer.resume(bytes_transferred);
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
