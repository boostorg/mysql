//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_STATEMENT_BASE_HPP
#define BOOST_MYSQL_STATEMENT_BASE_HPP

#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>
#include <boost/mysql/field_view.hpp>

#include <array>
#include <cstdint>

namespace boost {
namespace mysql {

/// Convenience constant to use when executing a statement without parameters.
constexpr std::array<field_view, 0> no_statement_params{};

/**
 * \brief Represents a prepared statement. See [link mysql.prepared_statements] for more info.
 * \details This class is a handle to a server-side prepared statement.
 *
 * The main use of a prepared statement is executing it
 * using [refmem statement_base execute], which yields a [reflink resultset_base].
 *
 * Prepared statements are default-constructible and movable, but not copyable.
 * [refmem statement_base valid] returns `false` for default-constructed
 * and moved-from prepared statements. Calling any member function on an invalid
 * prepared statements, other than assignment, results in undefined behavior.
 *
 * Prepared statements are managed by the server in a per-connection basis:
 * once created, a prepared statement may be used as long as the parent
 * [reflink connection] object (i.e. the connection that originated the resultset_base)
 * is alive and open. Calling any function on a statement_base
 * whose parent connection has been closed or destroyed results
 * in undefined behavior.
 */
class statement_base
{
public:
    /**
     * \brief Default constructor.
     * \details Default constructed statements have [refmem statement_base valid] return `false`.
     */
    statement_base() = default;

#ifndef BOOST_MYSQL_DOXYGEN
    // Private. Do not use. TODO: hide this
    void reset(void* channel, const detail::com_stmt_prepare_ok_packet& msg) noexcept
    {
        channel_ = channel;
        stmt_msg_ = msg;
    }

    void reset() noexcept { channel_ = nullptr; }
#endif

    /**
     * \brief Returns `true` if the statement is not a default-constructed object.
     * \details Calling any function other than assignment on an statement for which
     * this function returns `false` results in undefined behavior.
     */
    bool valid() const noexcept { return channel_ != nullptr; }

    /// Returns a server-side identifier for the statement (unique in a per-connection basis).
    std::uint32_t id() const noexcept
    {
        assert(valid());
        return stmt_msg_.statement_id;
    }

    /// Returns the number of parameters that should be provided when executing the statement.
    unsigned num_params() const noexcept
    {
        assert(valid());
        return stmt_msg_.num_params;
    }

protected:
    void* channel_ptr() noexcept { return channel_; }
    void swap(statement_base& other) noexcept { std::swap(*this, other); }

private:
    void* channel_{nullptr};
    detail::com_stmt_prepare_ok_packet stmt_msg_;
};

}  // namespace mysql
}  // namespace boost

#endif /* INCLUDE_BOOST_MYSQL_PREPARED_STATEMENT_HPP_ */
