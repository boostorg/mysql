//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_EXECUTE_RESPONSE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_EXECUTE_RESPONSE_HPP

#include <boost/mysql/diagnostics.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/read_resultset_head.hpp>
#include <boost/mysql/impl/internal/sansio/read_some_rows.hpp>

#include <boost/asio/coroutine.hpp>

namespace boost {
namespace mysql {
namespace detail {

class read_execute_response_algo : asio::coroutine
{
    read_resultset_head_algo read_head_st_;
    read_some_rows_algo read_some_rows_st_;

    connection_state_data& conn_state() { return read_head_st_.conn_state(); }

public:
    read_execute_response_algo(
        connection_state_data& st,
        diagnostics* diag,
        execution_processor* proc
    ) noexcept
        : read_head_st_(st, read_resultset_head_algo_params{diag, proc}),
          read_some_rows_st_(st, read_some_rows_algo_params{diag, proc, output_ref()})
    {
    }

    diagnostics& diag() noexcept { return *read_head_st_.params().diag; }
    execution_processor& processor() noexcept { return *read_head_st_.params().proc; }

    next_action resume(error_code ec)
    {
        next_action act;

        BOOST_ASIO_CORO_REENTER(*this)
        {
            while (!processor().is_complete())
            {
                if (processor().is_reading_head())
                {
                    read_head_st_ = read_resultset_head_algo(conn_state(), read_head_st_.params());
                    while (!(act = read_head_st_.resume(ec)).is_done())
                        BOOST_ASIO_CORO_YIELD return act;
                    if (act.error())
                        return act;
                }
                else if (processor().is_reading_rows())
                {
                    read_some_rows_st_ = read_some_rows_algo(conn_state(), read_some_rows_st_.params());
                    while (!(act = read_some_rows_st_.resume(ec)).is_done())
                        BOOST_ASIO_CORO_YIELD return act;
                    if (act.error())
                        return act;
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
