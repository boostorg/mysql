//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_PREPARE_STATEMENT_RESPONSE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_READ_PREPARE_STATEMENT_RESPONSE_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/sansio/sansio_algorithm.hpp>

namespace boost {
namespace mysql {
namespace detail {

class read_prepare_statement_response_algo : public sansio_algorithm, asio::coroutine
{
    diagnostics* diag_;
    std::uint8_t sequence_number_{0};
    unsigned remaining_meta_{0};
    statement res_;

    error_code process_response()
    {
        prepare_stmt_response response{};
        auto err = deserialize_prepare_stmt_response(st_->reader.message(), st_->flavor, response, *diag_);
        if (err)
            return err;
        res_ = access::construct<statement>(response.id, response.num_params);
        remaining_meta_ = response.num_columns + response.num_params;
        return error_code();
    }

public:
    read_prepare_statement_response_algo(connection_state_data& st, diagnostics* diag) noexcept
        : sansio_algorithm(st), diag_(diag)
    {
    }

    std::uint8_t& sequence_number() { return sequence_number_; }
    diagnostics& diag() { return *diag_; }

    next_action resume(error_code ec)
    {
        if (ec)
            return ec;

        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Note: diagnostics should have been cleaned by other algos

            // Read response
            BOOST_ASIO_CORO_YIELD return read(sequence_number_);

            // Process response
            ec = process_response();
            if (ec)
                return ec;

            // Server sends now one packet per parameter and field.
            // We ignore these for now.
            for (; remaining_meta_ > 0u; --remaining_meta_)
                BOOST_ASIO_CORO_YIELD return read(sequence_number_);
        }

        return next_action();
    }

    statement result() const noexcept { return res_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
