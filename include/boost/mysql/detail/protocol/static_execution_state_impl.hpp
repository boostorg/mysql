//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_STATIC_EXECUTION_STATE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_STATIC_EXECUTION_STATE_IMPL_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/deserialize_execute_response.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/mysql/detail/protocol/execution_processor.hpp>
#include <boost/mysql/detail/protocol/typed_helpers.hpp>
#include <boost/mysql/detail/typed/row_traits.hpp>

#include <array>
#include <cstddef>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

struct execst_parse_table_helper
{
    using fn_t = error_code (*)(void* data, std::size_t offset, const field_view* from);

    template <class RowType>
    static error_code parse_fn(void* data, std::size_t offset, const field_view* from)
    {
        RowType& obj = static_cast<RowType*>(data)[offset];
        return parse(from, obj);
    }

    template <class... RowType>
    static constexpr std::array<fn_t, sizeof...(RowType)> create_table() noexcept
    {
        return {&parse_fn<RowType>...};
    }
};

template <class... RowType>
class static_execution_state_impl : public typed_execution_state_base
{
    static constexpr std::size_t num_resultsets = sizeof...(RowType);
    static constexpr std::size_t num_columns[num_resultsets]{row_traits<RowType>::size...};

    struct ok_data
    {
        bool has_value{false};           // The OK packet information is default constructed, or actual data?
        std::uint64_t affected_rows{};   // OK packet data
        std::uint64_t last_insert_id{};  // OK packet data
        std::uint16_t warnings{};        // OK packet data
        bool is_out_params{false};       // Does this resultset contain OUT param information?
    };

    std::size_t resultset_index_{};
    std::size_t meta_index_{};
    std::array<metadata, get_max(num_columns)> meta_;
    std::array<field_view, get_max(num_columns)> temp_fields_{};
    ok_data eof_data_;
    std::vector<char> info_;
    output_ref output_;
    std::size_t read_rows_{};

    std::size_t current_num_columns() const noexcept { return num_columns[resultset_index_ - 1]; }

    error_code meta_check(diagnostics& diag) const
    {
        constexpr auto vtab = meta_check_table_helper::create<RowType...>();
        assert(should_read_rows());
        assert(resultset_index_ <= num_resultsets);
        return vtab[resultset_index_ - 1](meta(), diag);
    }

    void on_new_resultset() noexcept
    {
        assert(resultset_index_ < num_resultsets);
        ++resultset_index_;
        meta_index_ = 0;
        eof_data_ = ok_data{};
        info_.clear();
        read_rows_ = 0;
    }

    error_code on_ok_packet_impl(const ok_packet& pack)
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
            return resultset_index_ < num_resultsets ? error_code() : client_errc::num_resultsets_mismatch;
        }
        else
        {
            set_state(state_t::complete);
            return resultset_index_ == num_resultsets ? error_code() : client_errc::num_resultsets_mismatch;
        }
    }

public:
    static_execution_state_impl() = default;

    void reset_impl() noexcept override
    {
        resultset_index_ = 0;
        meta_index_ = 0;
        eof_data_ = ok_data();
        info_.clear();
        output_ = output_ref();
        read_rows_ = 0;
    }

    error_code on_head_ok_packet_impl(const ok_packet& pack) override
    {
        on_new_resultset();
        auto err = on_ok_packet_impl(pack);
        if (err)
            return err;
        return current_num_columns() == 0 ? error_code() : client_errc::num_columns_mismatch;
    }

    error_code on_num_meta_impl(std::size_t num_columns) override
    {
        on_new_resultset();
        if (num_columns != current_num_columns())
        {
            return client_errc::num_columns_mismatch;
        }
        set_state(state_t::reading_metadata);
        return error_code();
    }

    error_code on_meta_impl(const column_definition_packet& pack, diagnostics& diag) override
    {
        meta_[meta_index_++] = metadata_access::construct(pack, meta_mode() == metadata_mode::full);
        if (meta_index_ == current_num_columns())
        {
            set_state(state_t::reading_rows);
            return meta_check(diag);
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
        assert(output_.has_value());

        // deserialize the row
        auto err = deserialize_row(encoding(), ctx, meta(), temp_fields_.data());
        if (err)
            return err;

        // parse it into the output ref
        constexpr auto vtab = execst_parse_table_helper::create_table<RowType...>();
        err = vtab[resultset_index_ - 1](output_.data, read_rows_, temp_fields_.data());
        if (err)
            return err;

        ++read_rows_;
        return error_code();
    }

    bool has_space() const override
    {
        assert(output_.has_value());
        return read_rows_ < output_.max_size;
    }

    error_code set_output(output_ref ref) override
    {
        assert(should_read_rows());
        assert(ref.has_value());
        if (ref.resultset_number != resultset_index_ - 1)
            return client_errc::type_mismatch;
        output_ = ref;
        read_rows_ = 0;
        return error_code();
    }

    std::size_t num_read_rows() const noexcept override { return read_rows_; }

    // User facing
    metadata_collection_view meta() const noexcept
    {
        return metadata_collection_view(meta_.data(), current_num_columns());
    }

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
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
