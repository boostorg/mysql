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

#include <boost/mysql/impl/internal/sansio/reset_connection.hpp>

#include <boost/test/unit_test.hpp>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/buffer_concat.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_unit/algo_test.hpp"
#include "test_unit/create_err.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_ok.hpp"
#include "test_unit/create_ok_frame.hpp"
#include "test_unit/create_query_frame.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;

BOOST_AUTO_TEST_SUITE(test_reset_connection)

struct fixture : algo_fixture_base
{
    detail::reset_connection_algo algo;

    fixture(character_set charset = {}) : algo(st, {&diag, charset}) {}
};

BOOST_AUTO_TEST_CASE(success)
{
    // Setup
    fixture fix;
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, {0x1f}))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check(fix);

    // The OK packet was processed correctly. The charset was reset
    BOOST_TEST(fix.st.backslash_escapes);
    BOOST_TEST(fix.st.charset_ptr() == nullptr);
}

BOOST_AUTO_TEST_CASE(success_no_backslash_escapes)
{
    // Setup
    fixture fix;

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, {0x1f}))
        .expect_read(create_ok_frame(1, ok_builder().no_backslash_escapes(true).build()))
        .check(fix);

    // The OK packet was processed correctly.
    // It's OK to run this algo without a known charset
    BOOST_TEST(!fix.st.backslash_escapes);
    BOOST_TEST(fix.st.charset_ptr() == nullptr);
}

BOOST_AUTO_TEST_CASE(error_network)
{
    // This covers errors in read and write
    algo_test()
        .expect_write(create_frame(0, {0x1f}))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check_network_errors<fixture>();
}

BOOST_AUTO_TEST_CASE(error_response)
{
    // Setup
    fixture fix;
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo
    algo_test()
        .expect_write(create_frame(0, {0x1f}))
        .expect_read(err_builder()
                         .seqnum(1)
                         .code(common_server_errc::er_bad_db_error)
                         .message("my_message")
                         .build_frame())
        .check(fix, common_server_errc::er_bad_db_error, create_server_diag("my_message"));

    // The charset was not updated
    BOOST_TEST(fix.st.charset_ptr()->name == "utf8mb4");
}

BOOST_AUTO_TEST_CASE(charset_success)
{
    // Setup
    fixture fix(ascii_charset);
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo
    algo_test()
        .expect_write(concat_copy(create_frame(0, {0x1f}), create_query_frame(0, "SET NAMES 'ascii'")))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check(fix);

    // The OK packets were processed correctly. The charset is set
    BOOST_TEST(fix.st.backslash_escapes);
    BOOST_TEST(fix.st.charset_ptr()->name == "ascii");
}

BOOST_AUTO_TEST_CASE(charset_error_network)
{
    struct charset_fixture : fixture
    {
        charset_fixture() : fixture(ascii_charset) {}
    };

    // This covers errors in read and write
    algo_test()
        .expect_write(concat_copy(create_frame(0, {0x1f}), create_query_frame(0, "SET NAMES 'ascii'")))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check_network_errors<charset_fixture>();
}

BOOST_AUTO_TEST_CASE(charset_reset_error_response)
{
    // Setup
    fixture fix(ascii_charset);
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo
    algo_test()
        .expect_write(concat_copy(create_frame(0, {0x1f}), create_query_frame(0, "SET NAMES 'ascii'")))
        .expect_read(
            err_builder().seqnum(1).code(common_server_errc::er_cant_create_db).message("abc").build_frame()
        )
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .check(fix, common_server_errc::er_cant_create_db, create_server_diag("abc"));

    // Both packets are read, even if the first one is an error. The character set is updated because the
    // second packet is an OK
    BOOST_TEST(fix.st.backslash_escapes);
    BOOST_TEST(fix.st.charset_ptr()->name == "ascii");
}

BOOST_AUTO_TEST_CASE(charset_set_names_error_response)
{
    // Setup
    fixture fix(ascii_charset);
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo
    algo_test()
        .expect_write(concat_copy(create_frame(0, {0x1f}), create_query_frame(0, "SET NAMES 'ascii'")))
        .expect_read(create_ok_frame(1, ok_builder().build()))
        .expect_read(
            err_builder().seqnum(1).code(common_server_errc::er_cant_create_db).message("abc").build_frame()
        )
        .check(fix, common_server_errc::er_cant_create_db, create_server_diag("abc"));

    // Both packets are read. The character set is reset
    BOOST_TEST(fix.st.backslash_escapes);
    BOOST_TEST(fix.st.charset_ptr() == nullptr);
}

BOOST_AUTO_TEST_CASE(charset_all_error_responses)
{
    // Setup
    fixture fix(ascii_charset);
    fix.st.current_charset = utf8mb4_charset;

    // Run the algo
    algo_test()
        .expect_write(concat_copy(create_frame(0, {0x1f}), create_query_frame(0, "SET NAMES 'ascii'")))
        .expect_read(err_builder()
                         .seqnum(1)
                         .code(common_server_errc::er_autoinc_read_failed)
                         .message("hello")
                         .build_frame())
        .expect_read(
            err_builder().seqnum(1).code(common_server_errc::er_cant_create_db).message("abc").build_frame()
        )
        .check(fix, common_server_errc::er_autoinc_read_failed, create_server_diag("hello"));

    // Both packets are read. The character is not reset
    BOOST_TEST(fix.st.backslash_escapes);
    BOOST_TEST(fix.st.charset_ptr()->name == "utf8mb4");
}

BOOST_AUTO_TEST_CASE(charset_error_nonascii_charset_name)
{
    // Setup
    fixture fix({"lat\xc3\xadn", ascii_charset.next_char});

    // Run the algo
    algo_test().check(fix, client_errc::invalid_encoding);

    // The current character set was not updated
    BOOST_TEST(fix.st.charset_ptr() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
