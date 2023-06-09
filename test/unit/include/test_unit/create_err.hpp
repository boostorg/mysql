//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_ERR_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_CREATE_ERR_HPP

#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/string_view.hpp>

#include "protocol/basic_types.hpp"
#include "protocol/protocol.hpp"
#include "test_unit/create_frame.hpp"

namespace boost {
namespace mysql {
namespace test {

class err_builder
{
    std::uint16_t code_{};
    string_view msg_;
    std::uint8_t seqnum_{};

public:
    err_builder() = default;
    err_builder& code(std::uint16_t v) noexcept
    {
        code_ = v;
        return *this;
    }
    err_builder& code(common_server_errc v) noexcept { return code(static_cast<std::uint16_t>(v)); }
    err_builder& message(string_view v) noexcept
    {
        msg_ = v;
        return *this;
    }
    err_builder& seqnum(std::uint8_t v) noexcept
    {
        seqnum_ = v;
        return *this;
    }
    std::vector<std::uint8_t> build_body_without_header() const
    {
        return serialize_to_vector(
            code_,
            detail::string_fixed<1>{},  // SQL state marker
            detail::string_fixed<5>{},  // SQL state
            detail::string_eof{msg_}
        );
    }
    std::vector<std::uint8_t> build_body() const
    {
        return serialize_to_vector(
            std::uint8_t(0xff),  // header
            code_,
            detail::string_fixed<1>{},  // SQL state marker
            detail::string_fixed<5>{},  // SQL state
            detail::string_eof{msg_}
        );
    }
    std::vector<std::uint8_t> build_frame() const { return create_frame(seqnum_, build_body()); }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
