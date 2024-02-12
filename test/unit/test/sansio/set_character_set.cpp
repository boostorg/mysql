//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/sansio/set_character_set.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/create_diagnostics.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;

BOOST_AUTO_TEST_SUITE(test_set_character_set)

struct fixture : algo_fixture_base
{
    detail::set_character_set_algo algo;

    fixture(const character_set& charset = utf8mb4_charset) : algo(st, {&diag, charset}) {}
};

// GCC raises a spurious warning here
#if BOOST_GCC >= 110000
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overread"
#endif
static std::vector<std::uint8_t> create_query_body(string_view sql)
{
    std::vector<std::uint8_t> res{0x03};  // COM_QUERY command id
    const auto* data = reinterpret_cast<const std::uint8_t*>(sql.data());
    res.insert(res.end(), data, data + sql.size());
    return res;
}
#if BOOST_GCC >= 110000
#pragma GCC diagnostic pop
#endif

BOOST_AUTO_TEST_CASE(success)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, create_query_body("SET NAMES 'utf8mb4'")))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check(fix);

    // The charset was updated
    BOOST_TEST(fix.st.charset_ptr()->name == "utf8mb4");
}

BOOST_AUTO_TEST_CASE(success_previous_charset)
{
    // Setup
    fixture fix;
    fix.st.current_charset = ascii_charset;

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, create_query_body("SET NAMES 'utf8mb4'")))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check(fix);

    // The charset was updated
    BOOST_TEST(fix.st.charset_ptr()->name == "utf8mb4");
}

BOOST_AUTO_TEST_CASE(error_network)
{
    // This covers errors in read and write
    algo_test()
        .expect_write(create_frame(0, create_query_body("SET NAMES 'utf8mb4'")))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check_network_errors<fixture>();
}

BOOST_AUTO_TEST_CASE(error_response)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, create_query_body("SET NAMES 'utf8mb4'")))
        .expect_read(err_builder()
                         .seqnum(1)
                         .code(common_server_errc::er_unknown_character_set)
                         .message("Unknown charset")
                         .build_frame())
        .check(fix, common_server_errc::er_unknown_character_set, create_server_diag("Unknown charset"));

    // The current character set was not updated
    BOOST_TEST(fix.st.charset_ptr() == nullptr);
}

// Ensure we don't create vulnerabilities when composing SET NAMES
BOOST_AUTO_TEST_CASE(charset_name_needs_escaping)
{
    // Setup
    fixture fix({"lat'in\\", detail::next_char_latin1});

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, create_query_body("SET NAMES 'lat\\'in\\\\'")))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check(fix);
}

BOOST_AUTO_TEST_CASE(error_nonascii_charset_name)
{
    // Setup
    fixture fix({"lat\xc3\xadn", detail::next_char_latin1});

    // Run the algo
    algo_test().check(fix, client_errc::invalid_encoding);

    // The current character set was not updated
    BOOST_TEST(fix.st.charset_ptr() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
