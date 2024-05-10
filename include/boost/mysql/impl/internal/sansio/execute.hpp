//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_EXECUTE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_EXECUTE_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/any_execution_request.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/read_execute_response.hpp>
#include <boost/mysql/impl/internal/sansio/start_execution.hpp>

namespace boost {
namespace mysql {
namespace detail {

class execute_algo
{
    int resume_point_{0};
    start_execution_algo start_execution_st_;
    read_execute_response_algo read_response_st_;

    diagnostics& diag() { return read_response_st_.diag(); }
    execution_processor& processor() { return read_response_st_.processor(); }

public:
    execute_algo(connection_state_data& st, execute_algo_params params) noexcept
        : start_execution_st_(st, start_execution_algo_params{params.diag, params.req, params.proc}),
          read_response_st_(st, params.diag, params.proc)
    {
    }

    connection_state_data& conn_state() { return start_execution_st_.conn_state(); }

    next_action resume(error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            // Send request and read the first response
            while (!(act = start_execution_st_.resume(ec)).is_done())
                BOOST_MYSQL_YIELD(resume_point_, 1, act)
            if (act.error())
                return act;

            // Read anything else
            while (!(act = read_response_st_.resume(ec)).is_done())
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
