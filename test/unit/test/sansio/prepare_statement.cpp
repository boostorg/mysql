//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/statement.hpp>

#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>
#include <boost/mysql/impl/internal/sansio/prepare_statement.hpp>

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <vector>

#include "test_unit/algo_test.hpp"
#include "test_unit/create_coldef_frame.hpp"
#include "test_unit/create_frame.hpp"
#include "test_unit/create_meta.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql;

BOOST_AUTO_TEST_SUITE(test_prepare_statement)

class response_builder
{
    std::uint32_t statement_id_{1};
    std::uint16_t num_columns_{3};
    std::uint16_t num_params_{2};

public:
    response_builder() = default;
    response_builder& id(std::uint32_t v)
    {
        statement_id_ = v;
        return *this;
    }
    response_builder& num_columns(std::uint16_t v)
    {
        num_columns_ = v;
        return *this;
    }
    response_builder& num_params(std::uint16_t v)
    {
        num_params_ = v;
        return *this;
    }
    std::vector<std::uint8_t> build() const
    {
        std::vector<std::uint8_t> res;
        detail::serialization_context ctx(res, detail::disable_framing);
        ctx.serialize(
            detail::int1{0u},             // OK header
            detail::int4{statement_id_},  // statement_id
            detail::int2{num_columns_},   // num columns
            detail::int2{num_params_},    // num_params
            detail::int1{0u},             // reserved
            detail::int2{90u}             // warning_count
        );
        return create_frame(19, res);
    }
};

//
// read_prepare_statement_response_algo
//
struct read_response_fixture : algo_fixture_base
{
    detail::read_prepare_statement_response_algo algo{&diag, 19};

    // Clearing diagnostics is not this algorithm's responsibility
    read_response_fixture() : algo_fixture_base(diagnostics()) {}

    statement result() const { return algo.result(st); }
};

// OK
// Number of meta: 0, 1, 2
// Error deserializing response

BOOST_AUTO_TEST_CASE(read_response_success)
{
    // Setup
    read_response_fixture fix;

    // Run the algo
    algo_test()
        .expect_read(response_builder().id(1).num_columns(1).num_params(2).build())
        .expect_read(create_coldef_frame(20, meta_builder().name("abc").build_coldef()))
        .expect_read(create_coldef_frame(21, meta_builder().name("other").build_coldef()))
        .expect_read(create_coldef_frame(22, meta_builder().name("final").build_coldef()))
        .check(fix);

    // The statement was created successfully
    auto stmt = fix.result();
    BOOST_TEST(stmt.id() == 1u);
    BOOST_TEST(stmt.num_params() == 2u);
}

// BOOST_AUTO_TEST_CASE(read_response_error_network)
// {
//     algo_test()
//         .expect_read(create_ok_frame(11, ok_builder().build()))
//         .check_network_errors<read_response_fixture>();
// }

// BOOST_AUTO_TEST_CASE(read_response_error_packet)
// {
//     // Setup
//     read_response_fixture fix;
//     fix.st.current_charset = utf8mb4_charset;

//     // Run the algo
//     algo_test()
//         .expect_read(err_builder()
//                          .seqnum(11)
//                          .code(common_server_errc::er_bad_db_error)
//                          .message("my_message")
//                          .build_frame())
//         .check(fix, common_server_errc::er_bad_db_error, create_server_diag("my_message"));

//     // The charset was not updated
//     BOOST_TEST(fix.st.charset_ptr()->name == "utf8mb4");
// }

BOOST_AUTO_TEST_SUITE_END()
