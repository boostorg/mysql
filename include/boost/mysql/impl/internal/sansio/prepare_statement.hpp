//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PREPARE_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PREPARE_STATEMENT_HPP

#include <boost/mysql/detail/algo_params.hpp>

#include <boost/mysql/impl/internal/sansio/next_action.hpp>
#include <boost/mysql/impl/internal/sansio/read_prepare_statement_response.hpp>
#include <boost/mysql/impl/internal/sansio/sansio_algorithm.hpp>

#include <boost/asio/coroutine.hpp>

namespace boost {
namespace mysql {
namespace detail {

class prepare_statement_algo : public sansio_algorithm, asio::coroutine
{
    string_view stmt_sql_;
    read_prepare_statement_response_algo read_response_st_;

public:
    prepare_statement_algo(connection_state_data& st, prepare_statement_algo_params params) noexcept
        : sansio_algorithm(st), stmt_sql_(params.stmt_sql), read_response_st_(st, params.diag)
    {
    }

    next_action resume(error_code ec)
    {
        next_action act;

        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Clear diagnostics
            read_response_st_.diag().clear();

            // Send request
            BOOST_ASIO_CORO_YIELD return write(
                prepare_stmt_command{stmt_sql_},
                read_response_st_.sequence_number()
            );
            if (ec)
                return ec;

            // Read response
            while (!(act = read_response_st_.resume(ec)).is_done())
                BOOST_ASIO_CORO_YIELD return act;
            return act;
        }

        return next_action();
    }

    statement result() const noexcept { return read_response_st_.result(); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP_ */
