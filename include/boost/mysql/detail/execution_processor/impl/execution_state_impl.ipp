//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_IMPL_EXECUTION_STATE_IMPL_IPP
#define BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_IMPL_EXECUTION_STATE_IMPL_IPP

#pragma once

#include <boost/mysql/detail/auxiliar/row_impl.hpp>
#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>

void boost::mysql::detail::execution_state_impl::on_ok_packet_impl(const ok_packet& pack)
{
    eof_data_.has_value = true;
    eof_data_.affected_rows = pack.affected_rows.value;
    eof_data_.last_insert_id = pack.last_insert_id.value;
    eof_data_.warnings = pack.warnings;
    eof_data_.is_out_params = pack.status_flags & SERVER_PS_OUT_PARAMS;
    info_.assign(pack.info.value.begin(), pack.info.value.end());
}

void boost::mysql::detail::execution_state_impl::reset_impl() noexcept
{
    meta_.clear();
    eof_data_ = ok_data();
    info_.clear();
}

boost::mysql::error_code boost::mysql::detail::execution_state_impl::
    on_head_ok_packet_impl(const ok_packet& pack, diagnostics&)
{
    on_new_resultset();
    on_ok_packet_impl(pack);
    return error_code();
}

void boost::mysql::detail::execution_state_impl::on_num_meta_impl(std::size_t num_columns)
{
    on_new_resultset();
    meta_.reserve(num_columns);
}

boost::mysql::error_code boost::mysql::detail::execution_state_impl::
    on_meta_impl(metadata&& meta, string_view, bool, diagnostics&)
{
    meta_.push_back(std::move(meta));
    return error_code();
}

boost::mysql::error_code boost::mysql::detail::execution_state_impl::on_row_impl(
    deserialization_context&& ctx,
    const output_ref&,
    std::vector<field_view>& fields
)

{
    // add row storage
    span<field_view> storage = add_fields(fields, meta_.size());

    // deserialize the row
    return deserialize_row(encoding(), ctx, meta_, storage);
}

boost::mysql::error_code boost::mysql::detail::execution_state_impl::on_row_ok_packet_impl(
    const ok_packet& pack
)
{
    on_ok_packet_impl(pack);
    return error_code();
}

#endif
