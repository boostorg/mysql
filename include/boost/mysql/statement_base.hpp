//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_STATEMENT_BASE_HPP
#define BOOST_MYSQL_STATEMENT_BASE_HPP

#include <boost/mysql/field_view.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>

#include <array>
#include <cstdint>
#include <tuple>

namespace boost {
namespace mysql {

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
    /**
     * \brief Returns `true` if the object represents an actual server statement.
     * \details Calling any function other than assignment on a statement for which
     * this function returns `false` results in undefined behavior.
     *\n
     * To be usable for server communication, the \ref connection referenced by this object must be
     * alive and open, too.
     *\n
     * Returns `false` for default-constructed and closed statements.
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
    statement_base() = default;
    detail::channel_base* channel_ptr() noexcept { return channel_; }

private:
    detail::channel_base* channel_{nullptr};
    detail::com_stmt_prepare_ok_packet stmt_msg_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::statement_base_access;
#endif
};

}  // namespace mysql
}  // namespace boost

#include <boost/mysql/impl/statement_base.hpp>

#endif
