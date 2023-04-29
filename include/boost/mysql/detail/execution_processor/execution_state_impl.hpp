//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_EXECUTION_STATE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_EXECUTION_STATE_IMPL_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>

#include <boost/mysql/detail/auxiliar/row_impl.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>

#include <vector>

namespace boost {
namespace mysql {
namespace detail {

class execution_state_impl final : public execution_processor_with_output
{
    struct ok_data
    {
        bool has_value{false};           // The OK packet information is default constructed, or actual data?
        std::uint64_t affected_rows{};   // OK packet data
        std::uint64_t last_insert_id{};  // OK packet data
        std::uint16_t warnings{};        // OK packet data
        bool is_out_params{false};       // Does this resultset contain OUT param information?
    };

    std::size_t remaining_meta_{};
    std::vector<metadata> meta_;
    ok_data eof_data_;
    std::vector<char> info_;

    void clear_previous_resultset() noexcept
    {
        meta_.clear();
        eof_data_ = ok_data{};
        info_.clear();
    }

    void on_ok_packet_impl(const ok_packet& pack)
    {
        eof_data_.has_value = true;
        eof_data_.affected_rows = pack.affected_rows.value;
        eof_data_.last_insert_id = pack.last_insert_id.value;
        eof_data_.warnings = pack.warnings;
        eof_data_.is_out_params = pack.status_flags & SERVER_PS_OUT_PARAMS;
        info_.assign(pack.info.value.begin(), pack.info.value.end());
        if (pack.status_flags & SERVER_MORE_RESULTS_EXISTS)
        {
            set_state(state_t::reading_first_packet);
        }
        else
        {
            set_state(state_t::complete);
        }
    }

    std::vector<field_view>& fields() noexcept
    {
        assert(output().data);
        return *static_cast<std::vector<field_view>*>(output().data);
    }

public:
    execution_state_impl() = default;

    void reset_impl() noexcept override
    {
        remaining_meta_ = 0;
        meta_.clear();
        eof_data_ = ok_data();
        info_.clear();
        set_output(output_ref());
    }

    error_code on_head_ok_packet_impl(const ok_packet& pack, diagnostics&) override
    {
        clear_previous_resultset();
        on_ok_packet_impl(pack);
        return error_code();
    }

    error_code on_num_meta_impl(std::size_t num_columns) override
    {
        clear_previous_resultset();
        remaining_meta_ = num_columns;
        meta_.reserve(num_columns);
        set_state(state_t::reading_metadata);
        return error_code();
    }

    error_code on_meta_impl(const column_definition_packet& pack, diagnostics&) override
    {
        meta_.push_back(metadata_access::construct(pack, meta_mode() == metadata_mode::full));
        if (--remaining_meta_ == 0)
        {
            set_state(state_t::reading_rows);
        }
        return error_code();
    }

    error_code on_row_ok_packet_impl(const ok_packet& pack) override
    {
        on_ok_packet_impl(pack);
        return error_code();
    }

    error_code on_row_impl(deserialization_context& ctx) override
    {
        // add row storage
        field_view* storage = add_fields(fields(), meta_.size());

        // deserialize the row
        return deserialize_row(encoding(), ctx, meta_, storage);
    }

    void on_row_batch_start_impl() noexcept override final { fields().clear(); }

    void on_row_batch_finish_impl() noexcept override final {}

    std::size_t num_meta_impl() const noexcept override { return meta_.size(); }

    // User facing
    metadata_collection_view meta() const noexcept { return meta_; }

    std::uint64_t get_affected_rows() const noexcept
    {
        assert(eof_data_.has_value);
        return eof_data_.affected_rows;
    }

    std::uint64_t get_last_insert_id() const noexcept
    {
        assert(eof_data_.has_value);
        return eof_data_.last_insert_id;
    }

    unsigned get_warning_count() const noexcept
    {
        assert(eof_data_.has_value);
        return eof_data_.warnings;
    }

    string_view get_info() const noexcept
    {
        assert(eof_data_.has_value);
        return string_view(info_.data(), info_.size());
    }

    bool get_is_out_params() const noexcept
    {
        assert(eof_data_.has_value);
        return eof_data_.is_out_params;
    }

    execution_state_impl& get_interface() noexcept { return *this; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
