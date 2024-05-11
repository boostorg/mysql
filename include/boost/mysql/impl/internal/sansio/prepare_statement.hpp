//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PREPARE_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_PREPARE_STATEMENT_HPP

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/read_prepare_statement_response.hpp>

namespace boost {
namespace mysql {
namespace detail {

class prepare_statement_algo
{
    int resume_point_{0};
    read_prepare_statement_response_algo read_response_st_;
    string_view stmt_sql_;

public:
    prepare_statement_algo(prepare_statement_algo_params params) noexcept
        : read_response_st_(params.diag), stmt_sql_(params.stmt_sql)
    {
    }

    next_action resume(connection_state_data& st, error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            // Clear diagnostics
            read_response_st_.diag().clear();

            // Send request
            BOOST_MYSQL_YIELD(
                resume_point_,
                1,
                st.write(prepare_stmt_command{stmt_sql_}, read_response_st_.sequence_number())
            )
            if (ec)
                return ec;

            // Read response
            while (!(act = read_response_st_.resume(st, ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 2, act)
            return act;
        }

        return next_action();
    }

    statement result(const connection_state_data& st) const { return read_response_st_.result(st); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_HPP_ */
