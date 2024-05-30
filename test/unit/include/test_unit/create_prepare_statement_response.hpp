//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_PREPARE_STATEMENT_RESPONSE_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_PREPARE_STATEMENT_RESPONSE_HPP

#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>

#include <cstdint>
#include <vector>

#include "test_unit/create_frame.hpp"

namespace boost {
namespace mysql {
namespace test {

class prepare_stmt_response_builder
{
    std::uint8_t seqnum_{};
    std::uint32_t statement_id_{0};
    std::uint16_t num_columns_{0};
    std::uint16_t num_params_{0};

public:
    prepare_stmt_response_builder() = default;

    prepare_stmt_response_builder& seqnum(std::uint8_t v)
    {
        seqnum_ = v;
        return *this;
    }

    prepare_stmt_response_builder& id(std::uint32_t v)
    {
        statement_id_ = v;
        return *this;
    }

    prepare_stmt_response_builder& num_columns(std::uint16_t v)
    {
        num_columns_ = v;
        return *this;
    }

    prepare_stmt_response_builder& num_params(std::uint16_t v)
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
        return create_frame(seqnum_, res);
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
