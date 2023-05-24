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
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/auxiliar/row_impl.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>

#include <boost/assert.hpp>

#include <vector>

namespace boost {
namespace mysql {
namespace detail {

class execution_state_impl final : public execution_processor
{
    struct ok_data
    {
        bool has_value{false};           // The OK packet information is default constructed, or actual data?
        std::uint64_t affected_rows{};   // OK packet data
        std::uint64_t last_insert_id{};  // OK packet data
        std::uint16_t warnings{};        // OK packet data
        bool is_out_params{false};       // Does this resultset contain OUT param information?
    };

    std::vector<metadata> meta_;
    ok_data eof_data_;
    std::vector<char> info_;

    void on_new_resultset() noexcept
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
    }

    void reset_impl() noexcept override final
    {
        meta_.clear();
        eof_data_ = ok_data();
        info_.clear();
    }

    error_code on_head_ok_packet_impl(const ok_packet& pack, diagnostics&) override final
    {
        on_new_resultset();
        on_ok_packet_impl(pack);
        return error_code();
    }

    void on_num_meta_impl(std::size_t num_columns) override final
    {
        on_new_resultset();
        meta_.reserve(num_columns);
    }

    error_code on_meta_impl(metadata&& meta, string_view, bool, diagnostics&) override final
    {
        meta_.push_back(std::move(meta));
        return error_code();
    }

    error_code on_row_impl(deserialization_context ctx, const output_ref&, std::vector<field_view>& fields)
        override final
    {
        // add row storage
        span<field_view> storage = add_fields(fields, meta_.size());

        // deserialize the row
        return deserialize_row(encoding(), ctx, meta_, storage);
    }

    error_code on_row_ok_packet_impl(const ok_packet& pack) override final
    {
        on_ok_packet_impl(pack);
        return error_code();
    }

    void on_row_batch_start_impl() noexcept override final {}

    void on_row_batch_finish_impl() noexcept override final {}

public:
    execution_state_impl() = default;

    metadata_collection_view meta() const noexcept { return meta_; }

    std::uint64_t get_affected_rows() const noexcept
    {
        BOOST_ASSERT(eof_data_.has_value);
        return eof_data_.affected_rows;
    }

    std::uint64_t get_last_insert_id() const noexcept
    {
        BOOST_ASSERT(eof_data_.has_value);
        return eof_data_.last_insert_id;
    }

    unsigned get_warning_count() const noexcept
    {
        BOOST_ASSERT(eof_data_.has_value);
        return eof_data_.warnings;
    }

    string_view get_info() const noexcept
    {
        BOOST_ASSERT(eof_data_.has_value);
        return string_view(info_.data(), info_.size());
    }

    bool get_is_out_params() const noexcept
    {
        BOOST_ASSERT(eof_data_.has_value);
        return eof_data_.is_out_params;
    }

    execution_state_impl& get_interface() noexcept { return *this; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
