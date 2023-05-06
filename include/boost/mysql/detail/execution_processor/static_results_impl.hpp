//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_STATIC_RESULTS_IMPL_HPP
#define BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_STATIC_RESULTS_IMPL_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/mysql/detail/typing/cpp2db_map.hpp>
#include <boost/mysql/detail/typing/readable_field_traits.hpp>
#include <boost/mysql/detail/typing/row_traits.hpp>

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/integer_sequence.hpp>

#include <algorithm>
#include <array>
#include <cstddef>

namespace boost {
namespace mysql {
namespace detail {

using results_reset_fn_t = void (*)(void*);
using results_parse_fn_t = error_code (*)(const_cpp2db_t field_map, const field_view* from, void* to);

struct results_resultset_descriptor
{
    name_table_t name_table;
    meta_check_fn_t meta_check;
    results_parse_fn_t parse_fn;
};

struct static_per_resultset_data
{
    std::size_t meta_offset{};
    std::size_t meta_size{};
    std::size_t info_offset{};
    std::size_t info_size{};
    bool has_ok_packet_data{false};  // The OK packet information is default constructed, or actual data?
    std::uint64_t affected_rows{};   // OK packet data
    std::uint64_t last_insert_id{};  // OK packet data
    std::uint16_t warnings{};        // OK packet data
    bool is_out_params{false};       // Does this resultset contain OUT param information?
};

class results_external_data
{
public:
    results_external_data(
        span<const results_resultset_descriptor> desc,
        results_reset_fn_t reset,
        void* rows,
        field_view* temp_fields,
        std::size_t* pos_map,
        span<static_per_resultset_data> per_resultset
    ) noexcept
        : desc_(desc),
          reset_(reset),
          rows_(rows),
          temp_fields_(temp_fields),
          pos_map_(pos_map),
          per_resultset_(per_resultset.data())
    {
        assert(desc.size() == per_resultset.size());
    }

    results_external_data(const results_external_data&) = default;
    results_external_data(results_external_data&&) = default;
    results_external_data& operator=(const results_external_data&) noexcept { return *this; }
    results_external_data& operator=(results_external_data&&) noexcept { return *this; }
    ~results_external_data() = default;

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
    results_parse_fn_t parse_fn(std::size_t idx) const noexcept
    {
        assert(idx < num_resultsets());
        return desc_[idx].parse_fn;
    }
    results_reset_fn_t reset_fn() const noexcept { return reset_; }
    void* rows() const noexcept { return rows_; }
    field_view* temp_fields() const noexcept { return temp_fields_; }
    span<std::size_t> pos_map(std::size_t idx) const noexcept
    {
        return span<std::size_t>(pos_map_, num_columns(idx));
    }
    static_per_resultset_data& per_result(std::size_t idx) const noexcept
    {
        assert(idx < num_resultsets());
        return per_resultset_[idx];
    }

private:
    span<const results_resultset_descriptor> desc_;
    results_reset_fn_t reset_;
    void* rows_;
    field_view* temp_fields_;
    std::size_t* pos_map_;
    static_per_resultset_data* per_resultset_;
};

class static_results_erased_impl final : public execution_processor
{
public:
    static_results_erased_impl(results_external_data ext) noexcept : ext_(ext) {}

    metadata_collection_view get_meta(std::size_t index) const noexcept
    {
        const auto& resultset_data = ext_.per_result(index);
        return metadata_collection_view(meta_.data() + resultset_data.meta_offset, resultset_data.meta_size);
    }

    std::uint64_t get_affected_rows(std::size_t index) const noexcept
    {
        return get_resultset_with_ok_packet(index).affected_rows;
    }

    std::uint64_t get_last_insert_id(std::size_t index) const noexcept
    {
        return get_resultset_with_ok_packet(index).last_insert_id;
    }

    unsigned get_warning_count(std::size_t index) const noexcept
    {
        return get_resultset_with_ok_packet(index).warnings;
    }

    string_view get_info(std::size_t index) const noexcept
    {
        const auto& resultset_data = get_resultset_with_ok_packet(index);
        return string_view(info.data() + resultset_data.info_offset, resultset_data.info_size);
    }

    bool get_is_out_params(std::size_t index) const noexcept
    {
        return get_resultset_with_ok_packet(index).is_out_params;
    }

private:
    // Virtual implementations
    void reset_impl() noexcept override
    {
        ext_.reset_fn()(ext_.rows());
        info.clear();
        meta_.clear();
        meta_index = 0;
        resultset_index = 0;
    }

    error_code on_head_ok_packet_impl(const ok_packet& pack, diagnostics& diag) override final
    {
        add_resultset();
        auto err = on_ok_packet_impl(pack);
        if (err)
            return err;
        return meta_check(diag);
    }

    void on_num_meta_impl(std::size_t num_columns) override final
    {
        auto& resultset_data = add_resultset();
        meta_.reserve(meta_.size() + num_columns);
        resultset_data.meta_size = num_columns;
        set_state(state_t::reading_metadata);
    }

    error_code on_meta_impl(metadata&& meta, string_view field_name, diagnostics& diag) override final
    {
        // Fill the pos map entry for this field, if any
        cpp2db_add_field(current_pos_map(), current_name_table(), meta_index, field_name);

        // Store the meta object anyway
        meta_.push_back(std::move(meta));
        if (++meta_index == current_resultset().meta_size)
        {
            set_state(state_t::reading_rows);
            return meta_check(diag);
        }
        return error_code();
    }

    error_code on_row_ok_packet_impl(const ok_packet& pack) override final { return on_ok_packet_impl(pack); }

    error_code on_row_impl(deserialization_context ctx, const output_ref&) override final
    {
        // deserialize the row
        auto err = deserialize_row(encoding(), ctx, current_resultset_meta(), ext_.temp_fields());
        if (err)
            return err;

        // parse it against the appropriate tuple element
        return ext_.parse_fn(resultset_index - 1)(current_pos_map(), ext_.temp_fields(), ext_.rows());
    }

    void on_row_batch_start_impl() override final {}
    void on_row_batch_finish_impl() override final {}

    // Data
    results_external_data ext_;
    std::vector<metadata> meta_;
    std::vector<char> info;
    std::size_t meta_index{};
    std::size_t resultset_index{0};

    // Helpers
    cpp2db_t current_pos_map() noexcept { return ext_.pos_map(resultset_index - 1); }
    const_cpp2db_t current_pos_map() const noexcept { return ext_.pos_map(resultset_index - 1); }
    name_table_t current_name_table() const noexcept { return ext_.name_table(resultset_index - 1); }
    static_per_resultset_data& current_resultset() noexcept { return ext_.per_result(resultset_index - 1); }
    metadata_collection_view current_resultset_meta() const noexcept { return get_meta(resultset_index - 1); }

    static_per_resultset_data& add_resultset()
    {
        ++resultset_index;
        auto& resultset_data = current_resultset();
        resultset_data.meta_offset = meta_.size();
        meta_index = 0;
        cpp2db_reset(current_pos_map());
        return resultset_data;
    }

    error_code on_ok_packet_impl(const ok_packet& pack)
    {
        auto& resultset_data = current_resultset();
        resultset_data.affected_rows = pack.affected_rows.value;
        resultset_data.last_insert_id = pack.last_insert_id.value;
        resultset_data.warnings = pack.warnings;
        resultset_data.info_size = pack.info.value.size();
        resultset_data.has_ok_packet_data = true;
        resultset_data.is_out_params = pack.status_flags & SERVER_PS_OUT_PARAMS;
        info.insert(info.end(), pack.info.value.begin(), pack.info.value.end());
        bool is_last_resultset = resultset_index == ext_.num_resultsets();
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

    const static_per_resultset_data& get_resultset_with_ok_packet(std::size_t index) const noexcept
    {
        const auto& res = ext_.per_result(index);
        assert(res.has_ok_packet_data);
        return res;
    }

    error_code meta_check(diagnostics& diag) const
    {
        return ext_.meta_check_fn(resultset_index - 1)(current_pos_map(), current_resultset_meta(), diag);
    }
};

template <class... StaticRow>
using results_rows_t = std::tuple<std::vector<StaticRow>...>;

template <class... StaticRow>
struct results_fns
{
    using rows_t = results_rows_t<StaticRow...>;

    struct reset_fn
    {
        rows_t& obj;

        template <std::size_t I>
        void operator()(boost::mp11::mp_size_t<I>) const noexcept
        {
            std::get<I>(obj).clear();
        }
    };

    static void reset(void* rows_ptr) noexcept
    {
        auto& rows = *static_cast<rows_t*>(rows_ptr);
        boost::mp11::mp_for_each<boost::mp11::mp_iota_c<sizeof...(StaticRow)>>(reset_fn{rows});
    }

    template <std::size_t I>
    static error_code parse(const_cpp2db_t pos_map, const field_view* from, void* to)
    {
        auto& v = std::get<I>(*static_cast<rows_t*>(to));
        v.emplace_back();
        return parse(pos_map, from, v.back());
    }

    template <std::size_t I>
    static constexpr results_resultset_descriptor create_descriptor()
    {
        using T = mp11::mp_at_c<mp11::mp_list<StaticRow...>, I>;
        return {
            get_row_name_table<T>(),
            &meta_check<T>,
            &parse<I>,
        };
    }

    template <std::size_t... I>
    static constexpr std::array<results_resultset_descriptor, sizeof...(StaticRow)> create_descriptors(mp11::index_sequence<
                                                                                                       I...>)
    {
        return {create_descriptor<I>()...};
    }
};

template <class... StaticRow>
constexpr std::array<results_resultset_descriptor, sizeof...(StaticRow)>
    results_resultset_descriptor_table = results_fns<StaticRow...>::create_descriptors(
        mp11::make_index_sequence<sizeof...(StaticRow)>()
    );

template <BOOST_MYSQL_STATIC_ROW... StaticRow>
class static_results_impl
{
    // Data
    results_rows_t<StaticRow...> rows_;
    std::array<field_view, max_num_columns<StaticRow...>> temp_fields_{};
    std::array<std::size_t, max_num_columns<StaticRow...>> pos_table_{};
    std::array<static_per_resultset_data, sizeof...(StaticRow)> per_resultset_{};

    // The type-erased impl, that will use pointers to the above storage
    static_results_erased_impl impl_;

public:
    static_results_impl() noexcept
        : impl_(results_external_data(
              results_resultset_descriptor_table<StaticRow...>,
              &results_fns<StaticRow...>::reset,
              &rows_,
              temp_fields_.data(),
              pos_table_.data(),
              per_resultset_
          ))
    {
    }

    // User facing
    template <std::size_t I>
    boost::span<const typename std::tuple_element<I, std::tuple<StaticRow...>>::type> get_rows(
    ) const noexcept
    {
        return std::get<I>(rows_);
    }

    const static_results_erased_impl& get_interface() const noexcept { return impl_; }
    static_results_erased_impl& get_interface() noexcept { return impl_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
