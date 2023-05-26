//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_IMPL_STATIC_RESULTS_IMPL_IPP
#define BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_IMPL_STATIC_RESULTS_IMPL_IPP

#pragma once

#include <boost/mysql/client_errc.hpp>

#include <boost/mysql/detail/auxiliar/row_impl.hpp>
#include <boost/mysql/detail/execution_processor/static_results_impl.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/mysql/detail/typing/pos_map.hpp>

void boost::mysql::detail::static_results_erased_impl::reset_impl() noexcept
{
    ext_.reset_fn()(ext_.rows());
    info_.clear();
    meta_.clear();
    resultset_index_ = 0;
}

boost::mysql::error_code boost::mysql::detail::static_results_erased_impl::on_head_ok_packet_impl(
    const ok_packet& pack,
    diagnostics& diag
)
{
    add_resultset();
    auto err = on_ok_packet_impl(pack);
    if (err)
        return err;
    return meta_check(diag);
}

void boost::mysql::detail::static_results_erased_impl::on_num_meta_impl(std::size_t num_columns)
{
    auto& resultset_data = add_resultset();
    meta_.reserve(meta_.size() + num_columns);
    resultset_data.meta_size = num_columns;
}

boost::mysql::error_code boost::mysql::detail::static_results_erased_impl::on_meta_impl(
    metadata&& meta,
    string_view field_name,
    bool is_last,
    diagnostics& diag
)

{
    std::size_t meta_index = meta_.size() - current_resultset().meta_offset;

    // Store the new object
    meta_.push_back(std::move(meta));

    // Fill the pos map entry for this field, if any
    pos_map_add_field(current_pos_map(), current_name_table(), meta_index, field_name);

    return is_last ? meta_check(diag) : error_code();
}

boost::mysql::error_code boost::mysql::detail::static_results_erased_impl::on_row_impl(
    deserialization_context&& ctx,
    const output_ref&,
    std::vector<field_view>& fields
)

{
    auto meta = current_resultset_meta();

    // Allocate temporary storage
    fields.clear();
    span<field_view> storage = add_fields(fields, meta.size());

    // deserialize the row
    auto err = deserialize_row(encoding(), ctx, meta, storage);
    if (err)
        return err;

    // parse it against the appropriate tuple element
    return ext_.parse_fn(resultset_index_ - 1)(current_pos_map(), storage, ext_.rows());
}

boost::mysql::error_code boost::mysql::detail::static_results_erased_impl::on_row_ok_packet_impl(
    const ok_packet& pack
)
{
    return on_ok_packet_impl(pack);
}

boost::mysql::detail::static_per_resultset_data& boost::mysql::detail::static_results_erased_impl::
    add_resultset()
{
    ++resultset_index_;
    auto& resultset_data = current_resultset();
    resultset_data = static_per_resultset_data();
    resultset_data.meta_offset = meta_.size();
    resultset_data.info_offset = info_.size();
    pos_map_reset(current_pos_map());
    return resultset_data;
}

boost::mysql::error_code boost::mysql::detail::static_results_erased_impl::on_ok_packet_impl(
    const ok_packet& pack
)
{
    auto& resultset_data = current_resultset();
    resultset_data.affected_rows = pack.affected_rows.value;
    resultset_data.last_insert_id = pack.last_insert_id.value;
    resultset_data.warnings = pack.warnings;
    resultset_data.info_size = pack.info.value.size();
    resultset_data.has_ok_packet_data = true;
    resultset_data.is_out_params = pack.status_flags & SERVER_PS_OUT_PARAMS;
    info_.insert(info_.end(), pack.info.value.begin(), pack.info.value.end());
    bool should_be_last = resultset_index_ == ext_.num_resultsets();
    bool is_last = !(pack.status_flags & SERVER_MORE_RESULTS_EXISTS);
    return should_be_last == is_last ? error_code() : client_errc::num_resultsets_mismatch;
}

#endif
