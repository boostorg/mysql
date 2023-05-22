//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_DESERIALIZE_ROW_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_DESERIALIZE_ROW_HPP

#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_collection_view.hpp>

#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline error_code deserialize_row(
    resultset_encoding encoding,
    deserialization_context& ctx,
    metadata_collection_view meta,
    boost::span<field_view> output  // Should point to meta.size() field_view objects
);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#include <boost/mysql/detail/protocol/impl/deserialize_row.ipp>

#endif
