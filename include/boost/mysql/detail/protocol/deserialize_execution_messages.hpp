//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_DESERIALIZE_EXECUTION_MESSAGES_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_DESERIALIZE_EXECUTION_MESSAGES_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/db_flavor.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>

namespace boost {
namespace mysql {
namespace detail {

struct execute_response
{
    enum class type_t
    {
        num_fields,
        ok_packet,
        error
    } type;
    union data_t
    {
        static_assert(std::is_trivially_destructible<error_code>::value, "");

        std::size_t num_fields;
        ok_packet ok_pack;
        error_code err;

        data_t(size_t v) noexcept : num_fields(v) {}
        data_t(const ok_packet& v) noexcept : ok_pack(v) {}
        data_t(error_code v) noexcept : err(v) {}
    } data;

    execute_response(std::size_t v) noexcept : type(type_t::num_fields), data(v) {}
    execute_response(const ok_packet& v) noexcept : type(type_t::ok_packet), data(v) {}
    execute_response(error_code v) noexcept : type(type_t::error), data(v) {}
};

inline execute_response deserialize_execute_response(
    boost::asio::const_buffer msg,
    capabilities caps,
    db_flavor flavor,
    diagnostics& diag
) noexcept;

struct row_message
{
    enum class type_t
    {
        row,
        ok_packet,
        error
    } type;
    union data_t
    {
        static_assert(std::is_trivially_destructible<deserialization_context>::value, "");
        static_assert(std::is_trivially_destructible<ok_packet>::value, "");
        static_assert(std::is_trivially_destructible<error_code>::value, "");

        deserialization_context ctx;  // if row
        ok_packet ok_pack;
        error_code err;

        data_t(const deserialization_context& ctx) noexcept : ctx(ctx) {}
        data_t(const ok_packet& ok_pack) noexcept : ok_pack(ok_pack) {}
        data_t(error_code err) noexcept : err(err) {}
    } data;

    row_message(const deserialization_context& ctx) noexcept : type(type_t::row), data(ctx) {}
    row_message(const ok_packet& ok_pack) noexcept : type(type_t::ok_packet), data(ok_pack) {}
    row_message(error_code v) noexcept : type(type_t::error), data(v) {}
};

inline row_message deserialize_row_message(
    boost::asio::const_buffer msg,
    capabilities caps,
    db_flavor flavor,
    diagnostics& diag
);

inline row_message deserialize_row_message(
    channel_base& chan,
    std::uint8_t& sequence_number,
    diagnostics& diag
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/protocol/impl/deserialize_execution_messages.ipp>

#endif
