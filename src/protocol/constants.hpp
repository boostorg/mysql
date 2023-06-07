//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_SRC_PROTOCOL_CONSTANTS_HPP
#define BOOST_MYSQL_SRC_PROTOCOL_CONSTANTS_HPP

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

constexpr std::size_t MAX_PACKET_SIZE = 0xffffff;
constexpr std::size_t HEADER_SIZE = 4;

// Server status flags
constexpr std::uint32_t SERVER_STATUS_IN_TRANS = 1;
constexpr std::uint32_t SERVER_STATUS_AUTOCOMMIT = 2;
constexpr std::uint32_t SERVER_MORE_RESULTS_EXISTS = 8;
constexpr std::uint32_t SERVER_QUERY_NO_GOOD_INDEX_USED = 16;
constexpr std::uint32_t SERVER_QUERY_NO_INDEX_USED = 32;
constexpr std::uint32_t SERVER_STATUS_CURSOR_EXISTS = 64;
constexpr std::uint32_t SERVER_STATUS_LAST_ROW_SENT = 128;
constexpr std::uint32_t SERVER_STATUS_DB_DROPPED = 256;
constexpr std::uint32_t SERVER_STATUS_NO_BACKSLASH_ESCAPES = 512;
constexpr std::uint32_t SERVER_STATUS_METADATA_CHANGED = 1024;
constexpr std::uint32_t SERVER_QUERY_WAS_SLOW = 2048;
constexpr std::uint32_t SERVER_PS_OUT_PARAMS = 4096;
constexpr std::uint32_t SERVER_STATUS_IN_TRANS_READONLY = 8192;
constexpr std::uint32_t SERVER_SESSION_STATE_CHANGED = (1UL << 14);

// The binary collation number, used to distinguish blobs from strings
constexpr std::uint16_t binary_collation = 63;

// Prepared statements
namespace cursor_types {

constexpr std::uint8_t no_cursor = 0;
constexpr std::uint8_t read_only = 1;
constexpr std::uint8_t for_update = 2;
constexpr std::uint8_t scrollable = 4;

}  // namespace cursor_types

// Binary protocol (de)serialization constants
namespace binc {

constexpr std::size_t length_sz = 1;  // length byte, for date, datetime and time
constexpr std::size_t year_sz = 2;
constexpr std::size_t month_sz = 1;
constexpr std::size_t date_day_sz = 1;
constexpr std::size_t time_days_sz = 4;
constexpr std::size_t hours_sz = 1;
constexpr std::size_t mins_sz = 1;
constexpr std::size_t secs_sz = 1;
constexpr std::size_t micros_sz = 4;
constexpr std::size_t time_sign_sz = 1;

constexpr std::size_t date_sz = year_sz + month_sz + date_day_sz;  // does not include length

constexpr std::size_t datetime_d_sz = date_sz;
constexpr std::size_t datetime_dhms_sz = datetime_d_sz + hours_sz + mins_sz + secs_sz;
constexpr std::size_t datetime_dhmsu_sz = datetime_dhms_sz + micros_sz;

constexpr std::size_t time_dhms_sz = time_sign_sz + time_days_sz + hours_sz + mins_sz + secs_sz;
constexpr std::size_t time_dhmsu_sz = time_dhms_sz + micros_sz;

constexpr std::size_t time_max_days = 34;  // equivalent to the 839 hours, in the broken format

}  // namespace binc

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
