//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_OK_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_OK_HPP

#include <boost/mysql/string_view.hpp>

#include "protocol/basic_types.hpp"
#include "protocol/protocol.hpp"
#include "test_unit/create_frame.hpp"

namespace boost {
namespace mysql {
namespace test {

class ok_builder
{
    detail::ok_view ok_{};
    std::uint8_t seqnum_{};

    void flag(std::uint16_t f, bool value) noexcept
    {
        if (value)
            ok_.status_flags |= f;
        else
            ok_.status_flags &= ~f;
    }

    std::vector<std::uint8_t> build_body_impl(std::uint8_t header)
    {
        auto res = serialize_to_vector(
            std::uint8_t(header),
            detail::int_lenenc{ok_.affected_rows},
            detail::int_lenenc{ok_.last_insert_id},
            ok_.status_flags,
            ok_.warnings
        );
        // When info is empty, it's actually omitted in the ok_packet
        if (!ok_.info.empty())
        {
            serialize_to_vector(res, detail::string_lenenc{ok_.info});
        }
        return res;
    }

public:
    ok_builder() = default;
    ok_builder& affected_rows(std::uint64_t v) noexcept
    {
        ok_.affected_rows = v;
        return *this;
    }
    ok_builder& last_insert_id(std::uint64_t v) noexcept
    {
        ok_.last_insert_id = v;
        return *this;
    }
    ok_builder& warnings(std::uint16_t v) noexcept
    {
        ok_.warnings = v;
        return *this;
    }
    ok_builder& flags(std::uint16_t v) noexcept
    {
        ok_.status_flags = v;
        return *this;
    }
    ok_builder& more_results(bool v) noexcept
    {
        flag(detail::SERVER_MORE_RESULTS_EXISTS, v);
        return *this;
    }
    ok_builder& out_params(bool v) noexcept
    {
        flag(detail::SERVER_PS_OUT_PARAMS, v);
        return *this;
    }
    ok_builder& info(string_view v) noexcept
    {
        ok_.info = v;
        return *this;
    }
    ok_builder seqnum(std::uint8_t v)
    {
        seqnum_ = v;
        return *this;
    }
    detail::ok_view build() const noexcept { return ok_; }

    std::vector<std::uint8_t> build_ok_body() { return build_body_impl(0x00); }
    std::vector<std::uint8_t> build_eof_body() { return build_body_impl(0xfe); }
    std::vector<std::uint8_t> build_ok_frame() { return create_frame(seqnum_, build_ok_body()); }
    std::vector<std::uint8_t> build_eof_frame() { return create_frame(seqnum_, build_eof_body()); }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
