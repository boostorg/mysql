//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_ERR_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_ERR_HPP

#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/protocol/impl/protocol_types.hpp>
#include <boost/mysql/impl/internal/protocol/impl/serialization_context.hpp>

#include "test_unit/create_frame.hpp"

namespace boost {
namespace mysql {
namespace test {

inline std::vector<std::uint8_t> serialize_err_impl(detail::err_view pack, bool with_header)
{
    std::vector<std::uint8_t> buff;
    detail::serialization_context ctx(buff, detail::disable_framing);
    if (with_header)
        ctx.add(0xff);  // header
    ctx.serialize(
        detail::int2{pack.error_code},
        detail::string_fixed<1>{},  // SQL state marker
        detail::string_fixed<5>{},  // SQL state
        detail::string_eof{pack.error_message}
    );
    return buff;
}

class err_builder
{
    detail::err_view err_{};
    std::uint8_t seqnum_{};

public:
    err_builder() = default;
    err_builder& code(std::uint16_t v) noexcept
    {
        err_.error_code = v;
        return *this;
    }
    err_builder& code(common_server_errc v) noexcept { return code(static_cast<std::uint16_t>(v)); }
    err_builder& message(string_view v) noexcept
    {
        err_.error_message = v;
        return *this;
    }
    err_builder& seqnum(std::uint8_t v) noexcept
    {
        seqnum_ = v;
        return *this;
    }
    std::vector<std::uint8_t> build_body_without_header() const { return serialize_err_impl(err_, false); }
    std::vector<std::uint8_t> build_body() const { return serialize_err_impl(err_, true); }
    std::vector<std::uint8_t> build_frame() const { return create_frame(seqnum_, build_body()); }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
