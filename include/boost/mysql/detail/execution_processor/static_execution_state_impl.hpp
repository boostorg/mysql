//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_STATIC_EXECUTION_STATE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_STATIC_EXECUTION_STATE_IMPL_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/mysql/detail/typing/cpp2db_map.hpp>
#include <boost/mysql/detail/typing/row_traits.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

using execst_parse_fn_t = error_code (*)(const_cpp2db_t, const field_view* from, const output_ref& ref);

struct execst_resultset_descriptor
{
    name_table_t name_table;
    meta_check_fn_t meta_check;
    execst_parse_fn_t parse_fn;
};

class execst_external_data
{
public:
    execst_external_data(
        span<const execst_resultset_descriptor> desc,
        field_view* temp_fields,
        std::size_t* pos_map
    ) noexcept
        : desc_(desc), temp_fields_(temp_fields), pos_map_(pos_map)
    {
    }

    execst_external_data(const execst_external_data&) = default;
    execst_external_data(execst_external_data&&) = default;
    execst_external_data& operator=(const execst_external_data&) noexcept { return *this; }
    execst_external_data& operator=(execst_external_data&&) noexcept { return *this; }
    ~execst_external_data() = default;

    std::size_t num_resultsets() const noexcept { return desc_.size(); }
    std::size_t num_columns(std::size_t idx) const noexcept
    {
        assert(idx < num_resultsets());
        return desc_[idx].name_table.size();
    }
    name_table_t name_table(std::size_t idx) const noexcept
    {
        assert(idx < num_resultsets());
        return desc_[idx].name_table;
    }
    meta_check_fn_t meta_check_fn(std::size_t idx) const noexcept
    {
        assert(idx < num_resultsets());
        return desc_[idx].meta_check;
    }
    execst_parse_fn_t parse_fn(std::size_t idx) const noexcept
    {
        assert(idx < num_resultsets());
        return desc_[idx].parse_fn;
    }
    field_view* temp_fields() const noexcept { return temp_fields_; }
    span<std::size_t> pos_map(std::size_t idx) const noexcept
    {
        return span<std::size_t>(pos_map_, num_columns(idx));
    }

private:
    span<const execst_resultset_descriptor> desc_;
    field_view* temp_fields_;
    std::size_t* pos_map_;
};

class static_execution_state_erased_impl final : public execution_processor
{
public:
    static_execution_state_erased_impl(execst_external_data ext) noexcept : ext_(ext) {}
    metadata_collection_view meta() const noexcept { return meta_; }

    std::uint64_t get_affected_rows() const noexcept
    {
        assert(ok_data_.has_value);
        return ok_data_.affected_rows;
    }

    std::uint64_t get_last_insert_id() const noexcept
    {
        assert(ok_data_.has_value);
        return ok_data_.last_insert_id;
    }

    unsigned get_warning_count() const noexcept
    {
        assert(ok_data_.has_value);
        return ok_data_.warnings;
    }

    string_view get_info() const noexcept
    {
        assert(ok_data_.has_value);
        return string_view(info_.data(), info_.size());
    }

    bool get_is_out_params() const noexcept
    {
        assert(ok_data_.has_value);
        return ok_data_.is_out_params;
    }

private:
    // Data
    struct ok_packet_data
    {
        bool has_value{false};           // The OK packet information is default constructed, or actual data?
        std::uint64_t affected_rows{};   // OK packet data
        std::uint64_t last_insert_id{};  // OK packet data
        std::uint16_t warnings{};        // OK packet data
        bool is_out_params{false};       // Does this resultset contain OUT param information?
    };

    execst_external_data ext_;
    std::size_t resultset_index_{};
    std::size_t meta_index_{};
    std::size_t meta_size_{};
    ok_packet_data ok_data_;
    std::vector<char> info_;
    std::vector<metadata> meta_;

    // Virtual impls
    void reset_impl() noexcept override final
    {
        resultset_index_ = 0;
        meta_index_ = 0;
        meta_size_ = 0;
        ok_data_ = ok_packet_data();
        info_.clear();
        meta_.clear();
    }

    error_code on_head_ok_packet_impl(const ok_packet& pack, diagnostics& diag) override final
    {
        on_new_resultset();
        auto err = on_ok_packet_impl(pack);
        if (err)
            return err;
        return meta_check(diag);
    }

    void on_num_meta_impl(std::size_t num_columns) override final
    {
        on_new_resultset();
        meta_.reserve(num_columns);
        meta_size_ = num_columns;
        set_state(state_t::reading_metadata);
    }

    error_code on_meta_impl(metadata&& meta, string_view field_name, diagnostics& diag) override final
    {
        cpp2db_add_field(current_pos_map(), current_name_table(), meta_index_, field_name);

        // Store the meta object anyway
        meta_.push_back(std::move(meta));
        if (++meta_index_ == meta_size_)
        {
            set_state(state_t::reading_rows);
            return meta_check(diag);
        }
        return error_code();
    }

    error_code on_row_ok_packet_impl(const ok_packet& pack) override final { return on_ok_packet_impl(pack); }

    error_code on_row_impl(deserialization_context ctx, const output_ref& ref) override final
    {
        // check output
        if (ref.resultset_index() != resultset_index_ - 1)
            return client_errc::metadata_check_failed;  // TODO: is this OK?

        // deserialize the row
        auto err = deserialize_row(encoding(), ctx, meta(), ext_.temp_fields());
        if (err)
            return err;

        // parse it into the output ref
        err = ext_.parse_fn(resultset_index_ - 1)(current_pos_map(), ext_.temp_fields(), ref);
        if (err)
            return err;

        return error_code();
    }

    void on_row_batch_start_impl() noexcept override final {}

    void on_row_batch_finish_impl() noexcept override final {}

    // Auxiliar
    name_table_t current_name_table() const noexcept { return ext_.name_table(resultset_index_ - 1); }
    cpp2db_t current_pos_map() noexcept { return ext_.pos_map(resultset_index_ - 1); }
    const_cpp2db_t current_pos_map() const noexcept { return ext_.pos_map(resultset_index_ - 1); }

    error_code meta_check(diagnostics& diag) const
    {
        return ext_.meta_check_fn(resultset_index_ - 1)(current_pos_map(), meta_, diag);
    }

    void on_new_resultset() noexcept
    {
        ++resultset_index_;
        meta_index_ = 0;
        ok_data_ = ok_packet_data{};
        info_.clear();
        meta_.clear();
        cpp2db_reset(current_pos_map());
    }

    error_code on_ok_packet_impl(const ok_packet& pack)
    {
        ok_data_.has_value = true;
        ok_data_.affected_rows = pack.affected_rows.value;
        ok_data_.last_insert_id = pack.last_insert_id.value;
        ok_data_.warnings = pack.warnings;
        ok_data_.is_out_params = pack.status_flags & SERVER_PS_OUT_PARAMS;
        info_.assign(pack.info.value.begin(), pack.info.value.end());
        bool is_last_resultset = resultset_index_ == ext_.num_resultsets();
        if (pack.status_flags & SERVER_MORE_RESULTS_EXISTS)
        {
            set_state(state_t::reading_first_subseq);
            return is_last_resultset ? client_errc::num_resultsets_mismatch : error_code();
        }
        else
        {
            set_state(state_t::complete);
            return is_last_resultset ? error_code() : client_errc::num_resultsets_mismatch;
        }
    }
};

template <class StaticRow>
static error_code execst_parse_fn(const_cpp2db_t pos_map, const field_view* from, const output_ref& ref)
{
    return parse(pos_map, from, ref.span_element<StaticRow>());
}

template <class StaticRow>
constexpr execst_resultset_descriptor create_execst_resultset_descriptor()
{
    return {
        get_row_name_table<StaticRow>(),
        &meta_check<StaticRow>,
        &execst_parse_fn<StaticRow>,
    };
}

template <class... StaticRow>
constexpr std::array<execst_resultset_descriptor, sizeof...(StaticRow)> execst_resultset_descriptor_table{
    {create_execst_resultset_descriptor<StaticRow>()...}};

template <BOOST_MYSQL_STATIC_ROW... StaticRow>
class static_execution_state_impl
{
    // Storage for our data, which requires knowing the template args
    std::array<field_view, max_num_columns<StaticRow...>> temp_fields_{};
    std::array<std::size_t, max_num_columns<StaticRow...>> pos_map_{};

    // The type-erased impl, that will use pointers to the above storage
    static_execution_state_erased_impl impl_;

public:
    static_execution_state_impl() noexcept
        : impl_({execst_resultset_descriptor_table<StaticRow...>, temp_fields_.data(), pos_map_.data()})
    {
    }

    const static_execution_state_erased_impl& get_interface() const noexcept { return impl_; }
    static_execution_state_erased_impl& get_interface() noexcept { return impl_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
