//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/client_errc.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>
#include <boost/mysql/detail/execution_processor/results_impl.hpp>
#include <boost/mysql/detail/execution_processor/static_execution_state_impl.hpp>
#include <boost/mysql/detail/execution_processor/static_results_impl.hpp>
#include <boost/mysql/detail/row_impl.hpp>
#include <boost/mysql/detail/typing/pos_map.hpp>

#include "protocol/protocol.hpp"

using namespace boost::mysql;

void boost::mysql::detail::execution_processor::set_state_for_ok(const ok_view& pack) noexcept
{
    if (pack.more_results())
    {
        set_state(state_t::reading_first_subseq);
    }
    else
    {
        set_state(state_t::complete);
    }
}

// results_impl
detail::per_resultset_data& boost::mysql::detail::resultset_container::emplace_back()
{
    if (!first_has_data_)
    {
        first_ = per_resultset_data();
        first_has_data_ = true;
        return first_;
    }
    else
    {
        rest_.emplace_back();
        return rest_.back();
    }
}

row_view boost::mysql::detail::results_impl::get_out_params() const noexcept
{
    BOOST_ASSERT(is_complete());
    for (std::size_t i = 0; i < per_result_.size(); ++i)
    {
        if (per_result_[i].is_out_params)
        {
            auto res = get_rows(i);
            return res.empty() ? row_view() : res[0];
        }
    }
    return row_view();
}

void boost::mysql::detail::results_impl::reset_impl() noexcept
{
    meta_.clear();
    per_result_.clear();
    info_.clear();
    rows_.clear();
    num_fields_at_batch_start_ = no_batch;
}

void boost::mysql::detail::results_impl::on_num_meta_impl(std::size_t num_columns)
{
    auto& resultset_data = add_resultset();
    meta_.reserve(meta_.size() + num_columns);
    resultset_data.num_columns = num_columns;
}

error_code boost::mysql::detail::results_impl::on_head_ok_packet_impl(const ok_view& pack, diagnostics&)
{
    add_resultset();
    on_ok_packet_impl(pack);
    return error_code();
}

error_code boost::mysql::detail::results_impl::on_meta_impl(const coldef_view& coldef, bool, diagnostics&)
{
    meta_.push_back(create_meta(coldef));
    return error_code();
}

error_code boost::mysql::detail::results_impl::
    on_row_impl(span<const std::uint8_t> msg, const output_ref&, std::vector<field_view>&)
{
    BOOST_ASSERT(has_active_batch());

    // add row storage
    std::size_t num_fields = current_resultset().num_columns;
    span<field_view> storage = rows_.add_fields(num_fields);
    ++current_resultset().num_rows;

    // deserialize the row
    auto err = deserialize_row(encoding(), msg, current_resultset_meta(), storage);
    if (err)
        return err;

    return error_code();
}

error_code boost::mysql::detail::results_impl::on_row_ok_packet_impl(const ok_view& pack)
{
    on_ok_packet_impl(pack);
    return error_code();
}

void boost::mysql::detail::results_impl::on_row_batch_start_impl()
{
    BOOST_ASSERT(!has_active_batch());
    num_fields_at_batch_start_ = rows_.fields().size();
}

void boost::mysql::detail::results_impl::on_row_batch_finish_impl() { finish_batch(); }

void boost::mysql::detail::results_impl::finish_batch()
{
    if (has_active_batch())
    {
        rows_.copy_strings_as_offsets(
            num_fields_at_batch_start_,
            rows_.fields().size() - num_fields_at_batch_start_
        );
        num_fields_at_batch_start_ = no_batch;
    }
}

detail::per_resultset_data& boost::mysql::detail::results_impl::add_resultset()
{
    // Allocate a new per-resultset object
    auto& resultset_data = per_result_.emplace_back();
    resultset_data.meta_offset = meta_.size();
    resultset_data.field_offset = rows_.fields().size();
    resultset_data.info_offset = info_.size();
    return resultset_data;
}

void boost::mysql::detail::results_impl::on_ok_packet_impl(const ok_view& pack)
{
    auto& resultset_data = current_resultset();
    resultset_data.affected_rows = pack.affected_rows;
    resultset_data.last_insert_id = pack.last_insert_id;
    resultset_data.warnings = pack.warnings;
    resultset_data.info_size = pack.info.size();
    resultset_data.has_ok_packet_data = true;
    resultset_data.is_out_params = pack.is_out_params();
    info_.insert(info_.end(), pack.info.begin(), pack.info.end());
    if (!pack.more_results())
    {
        finish_batch();
        rows_.offsets_to_string_views();
    }
}

// static_results_impl
#ifdef BOOST_MYSQL_CXX14
void boost::mysql::detail::static_results_erased_impl::reset_impl() noexcept
{
    ext_.reset_fn()(ext_.rows());
    info_.clear();
    meta_.clear();
    resultset_index_ = 0;
}

error_code boost::mysql::detail::static_results_erased_impl::on_head_ok_packet_impl(
    const ok_view& pack,
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
    const coldef_view& coldef,
    bool is_last,
    diagnostics& diag
)

{
    std::size_t meta_index = meta_.size() - current_resultset().meta_offset;

    // Store the new object
    meta_.push_back(create_meta(coldef));

    // Fill the pos map entry for this field, if any
    pos_map_add_field(current_pos_map(), current_name_table(), meta_index, coldef.name);

    return is_last ? meta_check(diag) : error_code();
}

error_code boost::mysql::detail::static_results_erased_impl::on_row_impl(
    span<const std::uint8_t> msg,
    const output_ref&,
    std::vector<field_view>& fields
)

{
    auto meta = current_resultset_meta();

    // Allocate temporary storage
    fields.clear();
    span<field_view> storage = add_fields(fields, meta.size());

    // deserialize the row
    auto err = deserialize_row(encoding(), msg, meta, storage);
    if (err)
        return err;

    // parse it against the appropriate tuple element
    return ext_.parse_fn(resultset_index_ - 1)(current_pos_map(), storage, ext_.rows());
}

error_code boost::mysql::detail::static_results_erased_impl::on_row_ok_packet_impl(const ok_view& pack)
{
    return on_ok_packet_impl(pack);
}

detail::static_per_resultset_data& boost::mysql::detail::static_results_erased_impl::add_resultset()
{
    ++resultset_index_;
    auto& resultset_data = current_resultset();
    resultset_data = static_per_resultset_data();
    resultset_data.meta_offset = meta_.size();
    resultset_data.info_offset = info_.size();
    pos_map_reset(current_pos_map());
    return resultset_data;
}

error_code boost::mysql::detail::static_results_erased_impl::on_ok_packet_impl(const ok_view& pack)
{
    auto& resultset_data = current_resultset();
    resultset_data.affected_rows = pack.affected_rows;
    resultset_data.last_insert_id = pack.last_insert_id;
    resultset_data.warnings = pack.warnings;
    resultset_data.info_size = pack.info.size();
    resultset_data.has_ok_packet_data = true;
    resultset_data.is_out_params = pack.is_out_params();
    info_.insert(info_.end(), pack.info.begin(), pack.info.end());
    bool should_be_last = resultset_index_ == ext_.num_resultsets();
    bool is_last = !pack.more_results();
    return should_be_last == is_last ? error_code() : client_errc::num_resultsets_mismatch;
}
#endif

// execution_state_impl
void boost::mysql::detail::execution_state_impl::on_ok_packet_impl(const ok_view& pack)
{
    eof_data_.has_value = true;
    eof_data_.affected_rows = pack.affected_rows;
    eof_data_.last_insert_id = pack.last_insert_id;
    eof_data_.warnings = pack.warnings;
    eof_data_.is_out_params = pack.is_out_params();
    info_.assign(pack.info.begin(), pack.info.end());
}

void boost::mysql::detail::execution_state_impl::reset_impl() noexcept
{
    meta_.clear();
    eof_data_ = ok_data();
    info_.clear();
}

error_code boost::mysql::detail::execution_state_impl::
    on_head_ok_packet_impl(const ok_view& pack, diagnostics&)
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

error_code boost::mysql::detail::execution_state_impl::
    on_meta_impl(const coldef_view& coldef, bool, diagnostics&)
{
    meta_.push_back(create_meta(coldef));
    return error_code();
}

error_code boost::mysql::detail::execution_state_impl::on_row_impl(
    span<const std::uint8_t> msg,
    const output_ref&,
    std::vector<field_view>& fields
)

{
    // add row storage
    span<field_view> storage = add_fields(fields, meta_.size());

    // deserialize the row
    return deserialize_row(encoding(), msg, meta_, storage);
}

error_code boost::mysql::detail::execution_state_impl::on_row_ok_packet_impl(const ok_view& pack)
{
    on_ok_packet_impl(pack);
    return error_code();
}

// static_execution_state_impl
#ifdef BOOST_MYSQL_CXX14
void boost::mysql::detail::static_execution_state_erased_impl::reset_impl() noexcept
{
    resultset_index_ = 0;
    ok_data_ = ok_packet_data();
    info_.clear();
    meta_.clear();
}

error_code boost::mysql::detail::static_execution_state_erased_impl::on_head_ok_packet_impl(
    const ok_view& pack,
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

error_code boost::mysql::detail::static_execution_state_erased_impl::on_meta_impl(
    const coldef_view& coldef,
    bool is_last,
    diagnostics& diag
)

{
    std::size_t meta_index = meta_.size();

    // Store the object
    meta_.push_back(create_meta(coldef));

    // Record its position
    pos_map_add_field(current_pos_map(), current_name_table(), meta_index, coldef.name);

    return is_last ? meta_check(diag) : error_code();
}

error_code boost::mysql::detail::static_execution_state_erased_impl::on_row_impl(
    span<const std::uint8_t> msg,
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
    auto err = deserialize_row(encoding(), msg, meta_, storage);
    if (err)
        return err;

    // parse it into the output ref
    err = ext_.parse_fn(resultset_index_ - 1)(current_pos_map(), storage, ref);
    if (err)
        return err;

    return error_code();
}

error_code boost::mysql::detail::static_execution_state_erased_impl::on_row_ok_packet_impl(const ok_view& pack
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

error_code boost::mysql::detail::static_execution_state_erased_impl::on_ok_packet_impl(const ok_view& pack)
{
    ok_data_.has_value = true;
    ok_data_.affected_rows = pack.affected_rows;
    ok_data_.last_insert_id = pack.last_insert_id;
    ok_data_.warnings = pack.warnings;
    ok_data_.is_out_params = pack.is_out_params();
    info_.assign(pack.info.begin(), pack.info.end());
    bool should_be_last = resultset_index_ == ext_.num_resultsets();
    bool is_last = !pack.more_results();
    return should_be_last == is_last ? error_code() : client_errc::num_resultsets_mismatch;
}
#endif