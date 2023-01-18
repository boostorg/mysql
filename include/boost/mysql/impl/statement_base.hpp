//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_STATEMENT_BASE_HPP
#define BOOST_MYSQL_IMPL_STATEMENT_BASE_HPP

#pragma once

#include <boost/mysql/statement_base.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/channel/channel.hpp>

struct boost::mysql::detail::statement_base_access
{
    static void reset(
        statement_base& stmt,
        channel_base& channel,
        const detail::com_stmt_prepare_ok_packet& msg
    ) noexcept
    {
        stmt.channel_ = &channel;
        stmt.stmt_msg_ = msg;
    }

    static void reset(statement_base& stmt) noexcept { stmt.channel_ = nullptr; }
};

#endif
