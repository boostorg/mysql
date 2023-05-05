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

class static_execution_state_erased_impl final : public execution_processor
{
public:
    using parse_fn_t = error_code (*)(const_cpp2db_t, const field_view* from, const output_ref& ref);

    // These only depend on the type of the rows we're parsing
    struct resultset_descriptor
    {
        std::size_t num_resultsets{};
        const std::size_t* num_columns{};
        const name_table_t* name_table{};
        const meta_check_fn* meta_check_vtable{};
        const parse_fn_t* parse_vtable{};
    };

    // These point to storage owned by an object that knows the row types
    struct external_storage
    {
        field_view* temp_fields{};
        std::size_t* pos_map{};
    };

    // Both descriptor and storage must be provided on initialization
    static_execution_state_erased_impl(resultset_descriptor desc, external_storage ext) noexcept
        : desc_(desc), ext_(ext)
    {
    }

    // Copy/move construction requires providing the storage table for the object we're
    // creating, so we disable default copy/move construction. The descriptor table
    // shouldn't change
    static_execution_state_erased_impl(const static_execution_state_erased_impl& rhs) = delete;
    static_execution_state_erased_impl(const static_execution_state_erased_impl& rhs, external_storage st)
        : desc_(rhs.desc_), ext_(st), data_(rhs.data_)
    {
    }
    static_execution_state_erased_impl(static_execution_state_erased_impl&& rhs) = delete;
    static_execution_state_erased_impl(static_execution_state_erased_impl&& rhs, external_storage st) noexcept
        : desc_(rhs.desc_), ext_(st), data_(std::move(rhs.data_))
    {
    }

    // Assignment should only assign data, and not the descriptor or storage tables
    static_execution_state_erased_impl& operator=(const static_execution_state_erased_impl& rhs)
    {
        data_ = rhs.data_;
        return *this;
    }
    static_execution_state_erased_impl& operator=(static_execution_state_erased_impl&& rhs) noexcept
    {
        data_ = std::move(rhs.data_);
        return *this;
    }

    ~static_execution_state_erased_impl() = default;

    void reset_impl() noexcept override final
    {
        data_.resultset_index = 0;
        data_.meta_index = 0;
        data_.meta_size = 0;
        data_.ok_data = ok_packet_data();
        data_.info.clear();
        data_.meta.clear();
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
        data_.meta.reserve(num_columns);
        data_.meta_size = num_columns;
        set_state(state_t::reading_metadata);
    }

    error_code on_meta_impl(const column_definition_packet& pack, diagnostics& diag) override final
    {
        cpp2db_add_field(current_pos_map(), current_name_table(), data_.meta_index, pack.name.value);

        // Store the meta object anyway
        data_.meta.push_back(metadata_access::construct(pack, meta_mode() == metadata_mode::full));
        if (++data_.meta_index == data_.meta_size)
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
        if (ref.resultset_index() != data_.resultset_index - 1)
            return client_errc::metadata_check_failed;  // TODO: is this OK?

        // deserialize the row
        auto err = deserialize_row(encoding(), ctx, meta(), ext_.temp_fields);
        if (err)
            return err;

        // parse it into the output ref
        err = desc_.parse_vtable[data_.resultset_index - 1](current_pos_map(), ext_.temp_fields, ref);
        if (err)
            return err;

        return error_code();
    }

    void on_row_batch_start_impl() noexcept override final {}

    void on_row_batch_finish_impl() noexcept override final {}

    // User facing
    metadata_collection_view meta() const noexcept { return data_.meta; }

    std::uint64_t get_affected_rows() const noexcept
    {
        assert(data_.ok_data.has_value);
        return data_.ok_data.affected_rows;
    }

    std::uint64_t get_last_insert_id() const noexcept
    {
        assert(data_.ok_data.has_value);
        return data_.ok_data.last_insert_id;
    }

    unsigned get_warning_count() const noexcept
    {
        assert(data_.ok_data.has_value);
        return data_.ok_data.warnings;
    }

    string_view get_info() const noexcept
    {
        assert(data_.ok_data.has_value);
        return string_view(data_.info.data(), data_.info.size());
    }

    bool get_is_out_params() const noexcept
    {
        assert(data_.ok_data.has_value);
        return data_.ok_data.is_out_params;
    }

private:
    struct ok_packet_data
    {
        bool has_value{false};           // The OK packet information is default constructed, or actual data?
        std::uint64_t affected_rows{};   // OK packet data
        std::uint64_t last_insert_id{};  // OK packet data
        std::uint16_t warnings{};        // OK packet data
        bool is_out_params{false};       // Does this resultset contain OUT param information?
    };

    resultset_descriptor desc_;
    external_storage ext_;

    struct data_t
    {
        std::size_t resultset_index{};
        std::size_t meta_index{};
        std::size_t meta_size{};
        ok_packet_data ok_data;
        std::vector<char> info;
        std::vector<metadata> meta;
    } data_;

    std::size_t current_num_columns() const noexcept { return desc_.num_columns[data_.resultset_index - 1]; }
    name_table_t current_name_table() const noexcept
    {
        assert(desc_.name_table);
        return desc_.name_table[data_.resultset_index - 1];
    }
    cpp2db_t current_pos_map() noexcept { return cpp2db_t(ext_.pos_map, current_num_columns()); }
    const_cpp2db_t current_pos_map() const noexcept
    {
        return const_cpp2db_t(ext_.pos_map, current_num_columns());
    }

    error_code meta_check(diagnostics& diag) const
    {
        assert(data_.resultset_index <= desc_.num_resultsets);
        return desc_.meta_check_vtable[data_.resultset_index - 1](current_pos_map(), data_.meta, diag);
    }

    void on_new_resultset() noexcept
    {
        assert(data_.resultset_index < desc_.num_resultsets);
        ++data_.resultset_index;
        data_.meta_index = 0;
        data_.ok_data = ok_packet_data{};
        data_.info.clear();
        data_.meta.clear();
        cpp2db_reset(current_pos_map());
    }

    error_code on_ok_packet_impl(const ok_packet& pack)
    {
        data_.ok_data.has_value = true;
        data_.ok_data.affected_rows = pack.affected_rows.value;
        data_.ok_data.last_insert_id = pack.last_insert_id.value;
        data_.ok_data.warnings = pack.warnings;
        data_.ok_data.is_out_params = pack.status_flags & SERVER_PS_OUT_PARAMS;
        data_.info.assign(pack.info.value.begin(), pack.info.value.end());
        if (pack.status_flags & SERVER_MORE_RESULTS_EXISTS)
        {
            set_state(state_t::reading_first_subseq);
            return data_.resultset_index < desc_.num_resultsets ? error_code()
                                                                : client_errc::num_resultsets_mismatch;
        }
        else
        {
            set_state(state_t::complete);
            return data_.resultset_index == desc_.num_resultsets ? error_code()
                                                                 : client_errc::num_resultsets_mismatch;
        }
    }
};

template <class StaticRow>
static error_code static_execution_state_parse_fn(
    const_cpp2db_t pos_map,
    const field_view* from,
    const output_ref& ref
)
{
    return parse(pos_map, from, ref.span_element<StaticRow>());
}

template <class... StaticRow>
constexpr std::array<static_execution_state_erased_impl::parse_fn_t, sizeof...(StaticRow)>
    static_execution_state_parse_vtable{{&static_execution_state_parse_fn<StaticRow>...}};

template <BOOST_MYSQL_STATIC_ROW... StaticRow>
class static_execution_state_impl
{
    // Storage for our data, which requires knowing the template args
    struct data_t
    {
        std::array<field_view, max_num_columns<StaticRow...>> temp_fields{};
        std::array<std::size_t, max_num_columns<StaticRow...>> pos_map{};
    } data_;

    // The type-erased impl, that will use pointers to the above storage
    static_execution_state_erased_impl impl_;

    static static_execution_state_erased_impl::resultset_descriptor descriptor() noexcept
    {
        return {
            sizeof...(StaticRow),
            num_columns_table<StaticRow...>.data(),
            name_table<StaticRow...>.data(),
            meta_check_vtable<StaticRow...>.data(),
            static_execution_state_parse_vtable<StaticRow...>.data(),
        };
    }

    static_execution_state_erased_impl::external_storage storage_table() noexcept
    {
        return {
            data_.temp_fields.data(),
            data_.pos_map.data(),
        };
    }

public:
    static_execution_state_impl() noexcept : impl_(descriptor(), storage_table()) {}

    static_execution_state_impl(const static_execution_state_impl& rhs)
        : data_(rhs.data_), impl_(rhs.impl_, storage_table())
    {
    }

    static_execution_state_impl(static_execution_state_impl&& rhs) noexcept
        : data_(std::move(rhs.data_)), impl_(std::move(rhs.impl_, storage_table()))
    {
    }

    static_execution_state_impl& operator=(const static_execution_state_impl&) = default;
    static_execution_state_impl& operator=(static_execution_state_impl&&) = default;
    ~static_execution_state_impl() = default;

    const static_execution_state_erased_impl& get_interface() const noexcept { return impl_; }
    static_execution_state_erased_impl& get_interface() noexcept { return impl_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
