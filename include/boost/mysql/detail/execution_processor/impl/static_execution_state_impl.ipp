//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_IMPL_STATIC_EXECUTION_STATE_IMPL_IPP
#define BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_IMPL_STATIC_EXECUTION_STATE_IMPL_IPP

#pragma once

#include <boost/mysql/detail/auxiliar/row_impl.hpp>
#include <boost/mysql/detail/execution_processor/static_execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/mysql/detail/typing/pos_map.hpp>

void boost::mysql::detail::static_execution_state_erased_impl::reset_impl() noexcept
{
    resultset_index_ = 0;
    ok_data_ = ok_packet_data();
    info_.clear();
    meta_.clear();
}

boost::mysql::error_code boost::mysql::detail::static_execution_state_erased_impl::on_head_ok_packet_impl(
    const ok_packet& pack,
    diagnostics& diag
)
{
    on_new_resultset();
    auto err = on_ok_packet_impl(pack);
    if (err)
        return err;
    return meta_check(diag);
}

void boost::mysql::detail::static_execution_state_erased_impl::on_num_meta_impl(std::size_t num_columns)
{
    on_new_resultset();
    meta_.reserve(num_columns);
}

boost::mysql::error_code boost::mysql::detail::static_execution_state_erased_impl::on_meta_impl(
    metadata&& meta,
    string_view field_name,
    bool is_last,
    diagnostics& diag
)

{
    std::size_t meta_index = meta_.size();

    // Store the object
    meta_.push_back(std::move(meta));

    // Record its position
    pos_map_add_field(current_pos_map(), current_name_table(), meta_index, field_name);

    return is_last ? meta_check(diag) : error_code();
}

boost::mysql::error_code boost::mysql::detail::static_execution_state_erased_impl::on_row_impl(
    deserialization_context&& ctx,
    const output_ref& ref,
    std::vector<field_view>& fields
)

{
    // check output
    if (ref.type_index() != ext_.type_index(resultset_index_ - 1))
        return client_errc::row_type_mismatch;

    // Allocate temporary space
    fields.clear();
    span<field_view> storage = add_fields(fields, meta_.size());

    // deserialize the row
    auto err = deserialize_row(encoding(), ctx, meta_, storage);
    if (err)
        return err;

    // parse it into the output ref
    err = ext_.parse_fn(resultset_index_ - 1)(current_pos_map(), storage, ref);
    if (err)
        return err;

    return error_code();
}

boost::mysql::error_code boost::mysql::detail::static_execution_state_erased_impl::on_row_ok_packet_impl(
    const ok_packet& pack
)
{
    return on_ok_packet_impl(pack);
}

void boost::mysql::detail::static_execution_state_erased_impl::on_new_resultset() noexcept
{
    ++resultset_index_;
    ok_data_ = ok_packet_data{};
    info_.clear();
    meta_.clear();
    pos_map_reset(current_pos_map());
}

boost::mysql::error_code boost::mysql::detail::static_execution_state_erased_impl::on_ok_packet_impl(
    const ok_packet& pack
)
{
    ok_data_.has_value = true;
    ok_data_.affected_rows = pack.affected_rows.value;
    ok_data_.last_insert_id = pack.last_insert_id.value;
    ok_data_.warnings = pack.warnings;
    ok_data_.is_out_params = pack.status_flags & SERVER_PS_OUT_PARAMS;
    info_.assign(pack.info.value.begin(), pack.info.value.end());
    bool should_be_last = resultset_index_ == ext_.num_resultsets();
    bool is_last = !(pack.status_flags & SERVER_MORE_RESULTS_EXISTS);
    return should_be_last == is_last ? error_code() : client_errc::num_resultsets_mismatch;
}

#endif
