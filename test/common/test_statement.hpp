//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_TEST_STATEMENT_HPP
#define BOOST_MYSQL_TEST_COMMON_TEST_STATEMENT_HPP

#include <boost/mysql/statement.hpp>

#include <cstdint>

#include "test_connection.hpp"
#include "test_stream.hpp"

namespace boost {
namespace mysql {
namespace test {

using test_statement = statement<test_stream>;

inline test_statement create_statement(
    test_connection& conn,
    std::uint16_t num_params,
    std::uint32_t stmt_id = 1
)
{
    test_statement stmt{};
    stmt.reset(
        &conn.get_channel(),
        boost::mysql::detail::com_stmt_prepare_ok_packet{stmt_id, 2, num_params, 0}
    );
    return stmt;
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
