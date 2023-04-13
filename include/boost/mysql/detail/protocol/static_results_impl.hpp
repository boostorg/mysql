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

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>
#include <boost/mysql/detail/protocol/deserialize_execute_response.hpp>
#include <boost/mysql/detail/protocol/execution_processor.hpp>
#include <boost/mysql/detail/protocol/typed_helpers.hpp>
#include <boost/mysql/detail/typed/row_traits.hpp>

#include <boost/mp11/integer_sequence.hpp>

#include <array>

namespace boost {
namespace mysql {
namespace detail {

template <class... RowType>
struct results_parse_table_helper
{
    using tuple_t = std::tuple<std::vector<RowType>...>;
    using fn_t = error_code (*)(tuple_t&, const field_view*);
    using table_t = std::array<fn_t, sizeof...(RowType)>;

    template <std::size_t I>
    static error_code parse_fn(tuple_t& to, const field_view* from)
    {
        auto& v = std::get<I>(to);
        v.emplace_back();
        return detail::parse(from, v.back());
    }

    template <std::size_t... N>
    static constexpr table_t create_table_impl(boost::mp11::index_sequence<N...>)
    {
        return table_t{&parse_fn<N>...};
    }

    static constexpr table_t create_table()
    {
        return create_table_impl(boost::mp11::make_index_sequence<sizeof...(RowType)>());
    }
};

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

template <class... RowType>
class static_results_impl : public results_base
{
    using rows_t = std::tuple<std::vector<RowType>...>;
    static constexpr std::size_t num_resultsets = sizeof...(RowType);
    static constexpr std::size_t num_columns[num_resultsets]{row_traits<RowType>::size...};

    rows_t rows_;
    std::array<metadata, get_sum(num_columns)> meta_;
    std::array<basic_per_resultset_data, num_resultsets> per_resultset_;
    std::array<field_view, get_max(num_columns)> temp_fields_{};
    std::vector<char> info_;
    std::size_t meta_index_{};
    bool first_{true};
    std::size_t resultset_index_{0};

    struct reset_fn
    {
        rows_t& obj;

        template <std::size_t I>
        void operator()(boost::mp11::mp_size_t<I>) const noexcept
        {
            std::get<I>(obj).clear();
        }
    };

    void check_resultset_index() const { assert(resultset_index_ < num_resultsets); }

    std::size_t current_num_columns() const noexcept
    {
        check_resultset_index();
        return num_columns[resultset_index_];
    }

    void add_resultset()
    {
        if (first_)
        {
            first_ = false;
        }
        else
        {
            resultset_index_++;
        }
        check_resultset_index();
        meta_index_ = 0;
        auto& resultset_data = current_resultset();
        std::size_t offset = 0;
        for (std::size_t i = 0; i < resultset_index_; ++i)
            offset += num_columns[i];
        resultset_data.meta_offset = offset;
        resultset_data.info_offset = info_.size();
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
        info_.insert(info_.end(), pack.info.value.begin(), pack.info.value.end());
        if (pack.status_flags & SERVER_MORE_RESULTS_EXISTS)
        {
            set_state(state_t::reading_first_packet);
            return resultset_index_ < num_resultsets - 1 ? error_code()
                                                         : client_errc::num_resultsets_mismatch;
        }
        else
        {
            set_state(state_t::complete);
            return resultset_index_ == num_resultsets - 1 ? error_code()
                                                          : client_errc::num_resultsets_mismatch;
        }
    }

    basic_per_resultset_data& current_resultset() noexcept
    {
        check_resultset_index();
        return per_resultset_[resultset_index_];
    }
    metadata_collection_view current_resultset_meta() const noexcept
    {
        assert(should_read_rows());
        return get_meta(resultset_index_);
    }

    const basic_per_resultset_data& get_resultset_with_ok_packet(std::size_t index) const noexcept
    {
        assert(index < num_resultsets);
        const auto& res = per_resultset_[index];
        assert(res.has_ok_packet_data);
        return res;
    }

    error_code meta_check(diagnostics& diag) const
    {
        constexpr auto vtab = meta_check_table_helper::create<RowType...>();
        assert(should_read_rows());
        check_resultset_index();
        return vtab[resultset_index_](current_resultset_meta(), diag);
    }

public:
    static_results_impl() = default;

    void reset_impl() noexcept override
    {
        meta_index_ = 0;
        boost::mp11::mp_for_each<boost::mp11::mp_iota_c<num_resultsets>>(reset_fn{rows_});
        first_ = true;
        resultset_index_ = 0;
    }

    error_code on_head_ok_packet_impl(const ok_packet& pack) override
    {
        add_resultset();
        auto err = on_ok_packet_impl(pack);
        if (err)
            return err;
        return current_num_columns() == 0 ? error_code() : client_errc::num_columns_mismatch;
    }

    error_code on_num_meta_impl(std::size_t num_columns) override
    {
        add_resultset();
        if (num_columns != current_num_columns())
        {
            return client_errc::num_columns_mismatch;
        }
        set_state(state_t::reading_metadata);
        return error_code();
    }

    error_code on_meta_impl(const column_definition_packet& pack, diagnostics& diag) override
    {
        assert(meta_index_ < current_num_columns());
        meta_[current_resultset().meta_offset + meta_index_] = metadata_access::construct(
            pack,
            meta_mode() == metadata_mode::full
        );
        if (++meta_index_ == current_num_columns())
        {
            set_state(state_t::reading_rows);
            return meta_check(diag);
        }
        return error_code();
    }

    error_code on_row_ok_packet_impl(const ok_packet& pack) override { return on_ok_packet_impl(pack); }

    error_code on_row_impl(deserialization_context& ctx) override
    {
        // deserialize the row
        auto err = deserialize_row(encoding(), ctx, current_resultset_meta(), temp_fields_.data());
        if (err)
            return err;

        // parse it against the appropriate tuple element
        constexpr auto vtab = results_parse_table_helper<RowType...>::create_table();
        check_resultset_index();
        return vtab[resultset_index_](rows_, temp_fields_.data());
    }

    void on_row_batch_start() override {}
    void on_row_batch_finish() override {}

    // User facing
    template <std::size_t I>
    boost::span<const typename std::tuple_element<I, std::tuple<RowType...>>::type> get_rows() const noexcept
    {
        return std::get<I>(rows_);
    }

    metadata_collection_view get_meta(std::size_t index) const noexcept
    {
        assert(index < num_resultsets);
        return metadata_collection_view(meta_.data() + per_resultset_[index].meta_offset, num_columns[index]);
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
        return string_view(info_.data() + resultset_data.info_offset, resultset_data.info_size);
    }

    bool get_is_out_params(std::size_t index) const noexcept
    {
        return get_resultset_with_ok_packet(index).is_out_params;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
