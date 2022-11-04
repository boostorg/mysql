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
#include <tuple>

namespace boost {
namespace mysql {

/// Convenience constant to use when executing a statement without parameters.
constexpr std::tuple<> no_statement_params{};

/**
 * \brief The base class for prepared statements.
 * \details Don't instantiate this class directly - use \ref statement instead.
 *\n
 * All member functions, except otherwise noted, have `this->valid()` as precondition.
 * Calling any function on an invalid statement results in undefined behavior.
 */
class statement_base
{
public:
#ifndef BOOST_MYSQL_DOXYGEN
    statement_base() = default;
    // Private. Do not use. TODO: hide this
    void reset(void* channel, const detail::com_stmt_prepare_ok_packet& msg) noexcept
    {
        channel_ = channel;
        stmt_msg_ = msg;
    }

    void reset() noexcept { channel_ = nullptr; }
#endif

    /**
     * \brief Returns `true` if the object represents an actual server statement.
     * \details Calling any function other than assignment on a statement for which
     * this function returns `false` results in undefined behavior.
     *
     * To be usable for server communication, the \ref connection referenced by this object must be
     * alive and open, too.
     *
     * Returns `false` for default-constructed, moved-from and closed statements.
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

#endif
