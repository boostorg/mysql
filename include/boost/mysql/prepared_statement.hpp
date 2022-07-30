//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_PREPARED_STATEMENT_HPP
#define BOOST_MYSQL_PREPARED_STATEMENT_HPP


#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/value.hpp>
#include <array>
#include <cstdint>

namespace boost {
namespace mysql {

/// Convenience constant to use when executing a statement without parameters.
constexpr std::array<value, 0> no_statement_params {};

/**
 * \brief Represents a prepared statement. See [link mysql.prepared_statements] for more info.
 * \details This class is a handle to a server-side prepared statement.
 *
 * The main use of a prepared statement is executing it
 * using [refmem prepared_statement execute], which yields a [reflink resultset].
 *
 * Prepared statements are default-constructible and movable, but not copyable. 
 * [refmem prepared_statement valid] returns `false` for default-constructed 
 * and moved-from prepared statements. Calling any member function on an invalid
 * prepared statements, other than assignment, results in undefined behavior.
 *
 * Prepared statements are managed by the server in a per-connection basis:
 * once created, a prepared statement may be used as long as the parent
 * [reflink connection] object (i.e. the connection that originated the resultset)
 * is alive and open. Calling any function on a prepared_statement
 * whose parent connection has been closed or destroyed results
 * in undefined behavior.
 */
class prepared_statement
{
    bool valid_ {false};
    detail::com_stmt_prepare_ok_packet stmt_msg_;
public:
    /**
      * \brief Default constructor. 
      * \details Default constructed statements have [refmem prepared_statement valid] return `false`.
      */
    prepared_statement() = default;

#ifndef BOOST_MYSQL_DOXYGEN
    // Private. Do not use. TODO: hide this
    prepared_statement(const detail::com_stmt_prepare_ok_packet& msg) noexcept:
        valid_(true), stmt_msg_(msg) {}
#endif

    /**
     * \brief Returns `true` if the statement is not a default-constructed object.
     * \details Calling any function other than assignment on an statement for which
     * this function returns `false` results in undefined behavior.
     */
    bool valid() const noexcept { return valid_; }

    /// Returns a server-side identifier for the statement (unique in a per-connection basis).
    std::uint32_t id() const noexcept { assert(valid()); return stmt_msg_.statement_id; }

    /// Returns the number of parameters that should be provided when executing the statement.
    unsigned num_params() const noexcept { assert(valid()); return stmt_msg_.num_params; }

};

} // mysql
} // boost

#endif /* INCLUDE_BOOST_MYSQL_PREPARED_STATEMENT_HPP_ */
