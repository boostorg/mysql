//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_STATIC_RESULTS_IMPL_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_STATIC_RESULTS_IMPL_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_collection_view.hpp>

#include <boost/mysql/detail/protocol/deserialization_context.hpp>
#include <boost/mysql/detail/protocol/deserialize_row.hpp>
#include <boost/mysql/detail/protocol/execution_processor.hpp>
#include <boost/mysql/detail/protocol/typed_helpers.hpp>
#include <boost/mysql/detail/typed/row_traits.hpp>

#include <boost/mp11/integer_sequence.hpp>

#include <array>

namespace boost {
namespace mysql {
namespace detail {

class static_results_erased_impl : public execution_processor
{
public:
    using reset_fn_t = void (*)(void*);
    using parse_fn_t = error_code (*)(void* to, const field_view* from);

    struct basic_per_resultset_data
    {
        std::size_t meta_offset{};
        std::size_t info_offset{};
        std::size_t info_size{};
        bool has_ok_packet_data{false};  // The OK packet information is default constructed, or actual data?
        std::uint64_t affected_rows{};   // OK packet data
        std::uint64_t last_insert_id{};  // OK packet data
        std::uint16_t warnings{};        // OK packet data
        bool is_out_params{false};       // Does this resultset contain OUT param information?
    };

    // These only depend on the type of the rows we're parsing
    struct resultset_descriptor
    {
        std::size_t num_resultsets{};
        const std::size_t* num_columns{};
        reset_fn_t reset_fn{};
        const meta_check_fn* meta_check_vtable{};
        const parse_fn_t* parse_vtable{};
    };

    // These point to storage owned by an object that knows the row types
    struct external_storage
    {
        void* rows{};
        metadata* meta{};
        basic_per_resultset_data* per_resultset{};
        field_view* temp_fields{};
    };

    // Both descriptor and storage must be provided on initialization
    static_results_erased_impl(resultset_descriptor desc, external_storage ext) noexcept
        : desc_{desc}, ext_{ext}
    {
    }

    // Copy/move construction requires providing the storage table for the object we're
    // creating, so we disable default copy/move construction. The descriptor table
    // shouldn't change
    static_results_erased_impl(const static_results_erased_impl& rhs) = delete;
    static_results_erased_impl(const static_results_erased_impl& rhs, external_storage st)
        : desc_(rhs.desc_), ext_(st), data_(rhs.data_)
    {
    }
    static_results_erased_impl(static_results_erased_impl&& rhs) = delete;
    static_results_erased_impl(static_results_erased_impl&& rhs, external_storage st) noexcept
        : desc_(rhs.desc_), ext_(st), data_(std::move(rhs.data_))
    {
    }

    // Assignment should only assign data, and not the descriptor or storage tables
    static_results_erased_impl& operator=(const static_results_erased_impl& rhs)
    {
        data_ = rhs.data_;
        return *this;
    }
    static_results_erased_impl& operator=(static_results_erased_impl&& rhs) noexcept
    {
        data_ = std::move(rhs.data_);
        return *this;
    }

    ~static_results_erased_impl() = default;

    void reset_impl() noexcept override
    {
        desc_.reset_fn(ext_.rows);
        data_.meta_index = 0;
        data_.resultset_index = 0;
    }

    error_code on_head_ok_packet_impl(const ok_packet& pack) override final
    {
        add_resultset();
        auto err = on_ok_packet_impl(pack);
        if (err)
            return err;
        return current_num_columns() == 0 ? error_code() : client_errc::num_columns_mismatch;
    }

    error_code on_num_meta_impl(std::size_t num_columns) override final
    {
        add_resultset();
        if (num_columns != current_num_columns())
        {
            return client_errc::num_columns_mismatch;
        }
        set_state(state_t::reading_metadata);
        return error_code();
    }

    error_code on_meta_impl(const column_definition_packet& pack, diagnostics& diag) override final
    {
        assert(data_.meta_index < current_num_columns());
        ext_.meta[current_resultset().meta_offset + data_.meta_index] = metadata_access::construct(
            pack,
            meta_mode() == metadata_mode::full
        );
        if (++data_.meta_index == current_num_columns())
        {
            set_state(state_t::reading_rows);
            return meta_check(diag);
        }
        return error_code();
    }

    error_code on_row_impl(deserialization_context& ctx) override final
    {
        // deserialize the row
        auto err = deserialize_row(encoding(), ctx, current_resultset_meta(), ext_.temp_fields);
        if (err)
            return err;

        // parse it against the appropriate tuple element
        return desc_.parse_vtable[data_.resultset_index - 1](ext_.rows, ext_.temp_fields);
    }

    error_code on_row_ok_packet_impl(const ok_packet& pack) override final { return on_ok_packet_impl(pack); }

    void on_row_batch_start_impl() override final {}
    void on_row_batch_finish_impl() override final {}

    // User facing
    metadata_collection_view get_meta(std::size_t index) const noexcept
    {
        assert(index < desc_.num_resultsets);
        return metadata_collection_view(
            ext_.meta + ext_.per_resultset[index].meta_offset,
            desc_.num_columns[index]
        );
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
        return string_view(data_.info.data() + resultset_data.info_offset, resultset_data.info_size);
    }

    bool get_is_out_params(std::size_t index) const noexcept
    {
        return get_resultset_with_ok_packet(index).is_out_params;
    }

private:
    resultset_descriptor desc_;
    external_storage ext_;

    struct
    {
        std::vector<char> info;
        std::size_t meta_index{};
        bool first{true};
        std::size_t resultset_index{0};
    } data_;

    std::size_t current_num_columns() const noexcept { return desc_.num_columns[data_.resultset_index - 1]; }

    void add_resultset()
    {
        assert(data_.resultset_index < desc_.num_resultsets);
        ++data_.resultset_index;
        data_.meta_index = 0;
        auto& resultset_data = current_resultset();
        // TODO: we can do this constant
        std::size_t offset = 0;
        for (std::size_t i = 0; i < data_.resultset_index; ++i)
            offset += desc_.num_columns[i];
        resultset_data.meta_offset = offset;
        resultset_data.info_offset = data_.info.size();
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
        data_.info.insert(data_.info.end(), pack.info.value.begin(), pack.info.value.end());
        if (pack.status_flags & SERVER_MORE_RESULTS_EXISTS)
        {
            set_state(state_t::reading_first_packet);
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

    basic_per_resultset_data& current_resultset() noexcept
    {
        return ext_.per_resultset[data_.resultset_index - 1];
    }

    const basic_per_resultset_data& get_resultset_with_ok_packet(std::size_t index) const noexcept
    {
        assert(index < desc_.num_resultsets);
        const auto& res = ext_.per_resultset[index];
        assert(res.has_ok_packet_data);
        return res;
    }

    error_code meta_check(diagnostics& diag) const
    {
        assert(should_read_rows());
        return desc_.meta_check_vtable[data_.resultset_index - 1](current_resultset_meta(), diag);
    }

    metadata_collection_view current_resultset_meta() const noexcept
    {
        assert(should_read_rows());
        return get_meta(data_.resultset_index - 1);
    }
};

template <class... RowType>
class static_results_impl
{
    using rows_t = std::tuple<std::vector<RowType>...>;
    using parse_vtable_t = std::array<static_results_erased_impl::parse_fn_t, sizeof...(RowType)>;

    template <std::size_t I>
    static error_code parse_fn(void* to, const field_view* from)
    {
        auto& v = std::get<I>(*static_cast<rows_t*>(to));
        v.emplace_back();
        return detail::parse(from, v.back());
    }

    template <std::size_t... N>
    static constexpr parse_vtable_t create_parse_vtable_impl(boost::mp11::index_sequence<N...>)
    {
        return {&parse_fn<N>...};
    }

    static constexpr parse_vtable_t create_parse_vtable()
    {
        return create_parse_vtable_impl(boost::mp11::make_index_sequence<sizeof...(RowType)>());
    }

    static constexpr std::size_t num_resultsets = sizeof...(RowType);
    static constexpr std::size_t num_columns[num_resultsets]{row_traits<RowType>::size...};
    static constexpr auto meta_check_vtable = meta_check_table_helper::create<RowType...>();
    static constexpr parse_vtable_t parse_vtable = create_parse_vtable();

    // Storage for our data, which requires knowing the template args.
    struct data_t
    {
        rows_t rows;
        std::array<metadata, get_sum(num_columns)> meta{};
        std::array<static_results_erased_impl::basic_per_resultset_data, num_resultsets> per_resultset{};
        std::array<field_view, get_max(num_columns)> temp_fields{};
    } data_;

    // The type-erased impl, that will use pointers to the above storage
    static_results_erased_impl impl_;

    struct reset_fn
    {
        rows_t& obj;

        template <std::size_t I>
        void operator()(boost::mp11::mp_size_t<I>) const noexcept
        {
            std::get<I>(obj).clear();
        }
    };

    static void reset_tuple(void* rows_ptr) noexcept
    {
        auto& rows = *static_cast<rows_t*>(rows_ptr);
        boost::mp11::mp_for_each<boost::mp11::mp_iota_c<num_resultsets>>(reset_fn{rows});
    }

    static static_results_erased_impl::resultset_descriptor descriptor() noexcept
    {
        return {
            num_resultsets,
            num_columns,
            &reset_tuple,
            meta_check_vtable.data(),
            parse_vtable.data(),
        };
    }

    static_results_erased_impl::external_storage storage_table() noexcept
    {
        return {
            &data_.rows,
            data_.meta.data(),
            data_.per_resultset.data(),
            data_.temp_fields.data(),
        };
    }

public:
    static_results_impl() noexcept : impl_(descriptor(), storage_table()) {}

    static_results_impl(const static_results_impl& rhs) : data_(rhs.data_), impl_(rhs.impl_, storage_table())
    {
    }

    static_results_impl(static_results_impl&& rhs) noexcept
        : data_(std::move(rhs.data_)), impl_(std::move(rhs.impl_, storage_table()))
    {
    }

    static_results_impl& operator=(const static_results_impl&) = default;
    static_results_impl& operator=(static_results_impl&&) = default;
    ~static_results_impl() = default;

    // User facing
    template <std::size_t I>
    boost::span<const typename std::tuple_element<I, std::tuple<RowType...>>::type> get_rows() const noexcept
    {
        return std::get<I>(data_.rows);
    }

    const static_results_erased_impl& get_interface() const noexcept { return impl_; }
    static_results_erased_impl& get_interface() noexcept { return impl_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
