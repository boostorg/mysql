//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
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

#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>

#include "test_common/create_diagnostics.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/create_query_frame.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;

BOOST_AUTO_TEST_SUITE(test_set_character_set)

struct fixture : algo_fixture_base
{
    detail::set_character_set_algo algo;

    fixture(const character_set& charset = utf8mb4_charset) : algo(st, {&diag, charset}) {}
};

BOOST_AUTO_TEST_CASE(success)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(create_query_frame(0, "SET NAMES 'utf8mb4'"))
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
        .expect_write(create_query_frame(0, "SET NAMES 'utf8mb4'"))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check(fix);

    // The charset was updated
    BOOST_TEST(fix.st.charset_ptr()->name == "utf8mb4");
}

BOOST_AUTO_TEST_CASE(error_network)
{
    // This covers errors in read and write
    algo_test()
        .expect_write(create_query_frame(0, "SET NAMES 'utf8mb4'"))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check_network_errors<fixture>();
}

BOOST_AUTO_TEST_CASE(error_response)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(create_query_frame(0, "SET NAMES 'utf8mb4'"))
        .expect_read(err_builder()
                         .seqnum(1)
                         .code(common_server_errc::er_unknown_character_set)
                         .message("Unknown charset")
                         .build_frame())
        .check(fix, common_server_errc::er_unknown_character_set, create_server_diag("Unknown charset"));

    // The current character set was not updated
    BOOST_TEST(fix.st.charset_ptr() == nullptr);
}

// Character set function that always returns 1
static std::size_t stub_next_char(boost::span<const unsigned char>) noexcept { return 1u; }

// Ensure we don't create vulnerabilities when composing SET NAMES
BOOST_AUTO_TEST_CASE(charset_name_needs_escaping)
{
    // Setup
    fixture fix({"lat'in\\", stub_next_char});

    // Run the algo
    algo_test()
        .expect_write(create_query_frame(0, "SET NAMES 'lat\\'in\\\\'"))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check(fix);
}

BOOST_AUTO_TEST_CASE(error_nonascii_charset_name)
{
    // Setup
    fixture fix({"lat\xc3\xadn", stub_next_char});

    // Run the algo
    algo_test().check(fix, client_errc::invalid_encoding);

    // The current character set was not updated
    BOOST_TEST(fix.st.charset_ptr() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
