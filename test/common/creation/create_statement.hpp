//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_STATEMENT_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_STATEMENT_HPP

#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace test {

class statement_builder
{
    boost::mysql::detail::com_stmt_prepare_ok_packet pack_{};

public:
    statement_builder() = default;
    statement_builder& id(std::uint32_t v) noexcept
    {
        pack_.statement_id = v;
        return *this;
    }
    statement_builder& num_params(std::uint16_t v) noexcept
    {
        pack_.num_params = v;
        return *this;
    }
    statement build()
    {
        statement stmt;
        detail::statement_access::reset(stmt, pack_);
        return stmt;
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
