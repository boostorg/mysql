//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_EXECUTION_STATE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_EXECUTION_STATE_IMPL_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/typed.hpp>

#include <boost/mysql/detail/auxiliar/row_impl.hpp>
#include <boost/mysql/detail/auxiliar/string_view_offset.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/impl/rows_view.hpp>

#include <boost/core/span.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/integral.hpp>

#include <bits/utility.h>
#include <cassert>
#include <cstddef>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

// A rows-like collection that can hold the rows for multiple resultsets.
// This class also helps gathering all the rows without incurring in dangling references.
// Rows are read in batches:
// - Open a batch calling start_batch()
// - Call deserialize_row(), which will call add_row(). This adds raw storage for fields.
//   Fields are then deserialized there. string/blob field_view's point into the
//   connection's internal buffer.
// - When the batch finishes, call finish_batch(). This will copy strings into thr row_impl
//   and transform these values into offsets, so the buffer can be grown.
// - When the final OK packet is received, offsets are converted back into views,
//   by calling finish()
class multi_rows
{
    static constexpr std::size_t no_batch = std::size_t(-1);

    row_impl impl_;
    std::size_t num_fields_at_batch_start_{no_batch};

    bool has_active_batch() const noexcept { return num_fields_at_batch_start_ != no_batch; }

public:
    multi_rows() = default;

    std::size_t num_fields() const noexcept { return impl_.fields().size(); }

    rows_view rows_slice(std::size_t offset, std::size_t num_columns, std::size_t num_rows) const noexcept
    {
        return rows_view_access::construct(
            impl_.fields().data() + offset,
            num_rows * num_columns,
            num_columns
        );
    }

    void reset() noexcept
    {
        impl_.clear();
        num_fields_at_batch_start_ = no_batch;
    }

    void start_batch() noexcept
    {
        assert(!has_active_batch());
        num_fields_at_batch_start_ = num_fields();
    }

    field_view* add_row(std::size_t num_fields)
    {
        assert(has_active_batch());
        return impl_.add_fields(num_fields);
    }

    void finish_batch()
    {
        if (has_active_batch())
        {
            impl_.copy_strings_as_offsets(
                num_fields_at_batch_start_,
                impl_.fields().size() - num_fields_at_batch_start_
            );
            num_fields_at_batch_start_ = no_batch;
        }
    }

    void finish()
    {
        finish_batch();
        impl_.offsets_to_string_views();
    }
};

struct per_resultset_data
{
    std::size_t num_columns{};       // Number of columns this resultset has
    std::size_t meta_offset{};       // Offset into the vector of metadata
    std::size_t field_offset;        // Offset into the vector of fields (append mode only)
    std::size_t num_rows{};          // Number of rows this resultset has (append mode only)
    std::uint64_t affected_rows{};   // OK packet data
    std::uint64_t last_insert_id{};  // OK packet data
    std::uint16_t warnings{};        // OK packet data
    std::size_t info_offset{};       // Offset into the vector of info characters
    std::size_t info_size{};         // Number of characters that this resultset's info string has
    bool has_ok_packet_data{false};  // The OK packet information is default constructed, or actual data?
    bool is_out_params{false};       // Does this resultset contain OUT param information?
};

// A container similar to a vector with SBO. To avoid depending on Boost.Container
class resultset_container
{
    bool first_has_data_{false};
    per_resultset_data first_;
    std::vector<per_resultset_data> rest_;

public:
    resultset_container() = default;
    std::size_t size() const noexcept { return !first_has_data_ ? 0 : rest_.size() + 1; }
    bool empty() const noexcept { return !first_has_data_; }
    void clear() noexcept
    {
        first_has_data_ = false;
        rest_.clear();
    }
    per_resultset_data& operator[](std::size_t i) noexcept
    {
        return const_cast<per_resultset_data&>(const_cast<const resultset_container&>(*this)[i]);
    }
    const per_resultset_data& operator[](std::size_t i) const noexcept
    {
        assert(i < size());
        return i == 0 ? first_ : rest_[i - 1];
    }
    per_resultset_data& back() noexcept
    {
        return const_cast<per_resultset_data&>(const_cast<const resultset_container&>(*this).back());
    }
    const per_resultset_data& back() const noexcept
    {
        assert(first_has_data_);
        return rest_.empty() ? first_ : rest_.back();
    }
    per_resultset_data& emplace_back()
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
};

class execution_state_iface
{
public:
    virtual ~execution_state_iface() {}
    bool initial() const noexcept { return state_ == state_t::initial; }
    bool should_read_head() const noexcept
    {
        return state_ == state_t::initial || state_ == state_t::reading_first_packet;
    }
    bool should_read_head_subsequent() const noexcept { return state_ == state_t::reading_first_packet; }
    bool should_read_meta() const noexcept { return state_ == state_t::reading_metadata; }
    bool should_read_rows() const noexcept { return state_ == state_t::reading_rows; }
    bool complete() const noexcept { return state_ == state_t::complete; }
    resultset_encoding encoding() const noexcept { return encoding_; }
    std::uint8_t& sequence_number() noexcept { return seqnum_; }

    virtual void reset(resultset_encoding enc) noexcept = 0;
    virtual error_code on_num_meta(std::size_t num_meta) = 0;
    virtual void on_meta(const column_definition_packet& pack, metadata_mode mode) = 0;
    virtual error_code meta_check(diagnostics& diag) const = 0;
    virtual void on_row_batch_start() = 0;
    virtual bool can_add_row() const = 0;
    virtual field_view* add_row_storage() = 0;
    virtual error_code parse_row() = 0;
    virtual void on_row_batch_finish() = 0;
    virtual error_code on_head_ok_packet(const ok_packet& pack) = 0;
    virtual error_code on_row_ok_packet(const ok_packet& pack) = 0;
    virtual metadata_collection_view current_resultset_meta() const noexcept = 0;

protected:
    enum class state_t
    {
        initial,
        reading_first_packet,  // we're waiting for a subsequent resultset's 1st packet
        reading_metadata,
        reading_rows,
        complete
    };
    state_t state() const noexcept { return state_; }
    void set_state(state_t v) noexcept { state_ = v; }
    void set_encoding(resultset_encoding v) noexcept { encoding_ = v; }
    void reset_base(resultset_encoding enc) noexcept
    {
        state_ = state_t::initial;
        encoding_ = enc;
        seqnum_ = 0;
    }

private:
    state_t state_;
    resultset_encoding encoding_{resultset_encoding::text};
    std::uint8_t seqnum_{};
};

class results_impl : public execution_state_iface
{
    std::size_t remaining_meta_{};
    std::vector<metadata> meta_;
    resultset_container per_result_;
    std::vector<char> info_;
    multi_rows rows_;

    per_resultset_data& add_resultset()
    {
        // Allocate a new per-resultset object
        auto& resultset_data = per_result_.emplace_back();
        resultset_data.meta_offset = meta_.size();
        resultset_data.field_offset = rows_.num_fields();
        resultset_data.info_offset = info_.size();
        return resultset_data;
    }

    void on_ok_packet_impl(const ok_packet& pack)
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
        }
        else
        {
            rows_.finish();
            set_state(state_t::complete);
        }
    }

    const per_resultset_data& get_resultset_with_ok_packet(std::size_t index) const noexcept
    {
        const auto& res = per_result_[index];
        assert(res.has_ok_packet_data);
        return res;
    }

public:
    results_impl() = default;

    void reset(resultset_encoding enc) noexcept override
    {
        reset_base(enc);
        remaining_meta_ = 0;
        meta_.clear();
        per_result_.clear();
        info_.clear();
    }

    error_code on_num_meta(std::size_t num_columns) override
    {
        assert(should_read_head());
        auto& resultset_data = add_resultset();
        meta_.reserve(meta_.size() + num_columns);
        resultset_data.num_columns = num_columns;
        remaining_meta_ = num_columns;
        set_state(state_t::reading_metadata);
        return error_code();
    }

    void on_meta(const column_definition_packet& pack, metadata_mode mode) override
    {
        assert(should_read_meta());
        meta_.push_back(metadata_access::construct(pack, mode == metadata_mode::full));
        if (--remaining_meta_ == 0)
        {
            set_state(state_t::reading_rows);
        }
    }

    error_code meta_check(diagnostics&) const override { return error_code(); }

    void on_row_batch_start() override
    {
        assert(state() == state_t::reading_rows);
        rows_.start_batch();
    }

    bool can_add_row() const override { return true; }

    field_view* add_row_storage() override
    {
        assert(state() == state_t::reading_rows);
        std::size_t num_fields = current_resultset().num_columns;
        field_view* res = rows_.add_row(num_fields);
        ++current_resultset().num_rows;
        return res;
    }

    error_code parse_row() override { return error_code(); }

    void on_row_batch_finish() override { rows_.finish_batch(); }

    error_code on_head_ok_packet(const ok_packet& pack) override
    {
        assert(should_read_head());
        add_resultset();
        on_ok_packet_impl(pack);
        return error_code();
    }

    error_code on_row_ok_packet(const ok_packet& pack) override
    {
        assert(state() == state_t::reading_rows);
        on_ok_packet_impl(pack);
        return error_code();
    }

    metadata_collection_view current_resultset_meta() const noexcept override
    {
        assert(state() == state_t::reading_rows);
        return get_meta(per_result_.size() - 1);
    }

    // User facing
    row_view get_out_params() const noexcept
    {
        assert(complete());
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

    std::size_t num_resultsets() const noexcept { return per_result_.size(); }

    rows_view get_rows(std::size_t index) const noexcept
    {
        const auto& resultset_data = per_result_[index];
        return rows_
            .rows_slice(resultset_data.field_offset, resultset_data.num_columns, resultset_data.num_rows);
    }
    metadata_collection_view get_meta(std::size_t index) const noexcept
    {
        const auto& resultset_data = per_result_[index];
        return metadata_collection_view(
            meta_.data() + resultset_data.meta_offset,
            resultset_data.num_columns
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
        return string_view(info_.data() + resultset_data.info_offset, resultset_data.info_size);
    }

    bool get_is_out_params(std::size_t index) const noexcept
    {
        return get_resultset_with_ok_packet(index).is_out_params;
    }

private:
    per_resultset_data& current_resultset() noexcept
    {
        assert(!per_result_.empty());
        return per_result_.back();
    }

    const per_resultset_data& current_resultset() const noexcept
    {
        assert(!per_result_.empty());
        return per_result_.back();
    }
};

class row_reader
{
public:
    virtual ~row_reader() {}
    virtual void clear() = 0;
    virtual error_code meta_check(metadata_collection_view meta, diagnostics& diag) = 0;
    virtual bool has_space() const = 0;
    virtual field_view* add_row(std::size_t num_columns) = 0;
    virtual error_code parse_row() = 0;
};

class field_row_reader : public row_reader
{
    std::vector<field_view>* storage_;

    std::vector<field_view>& get() noexcept
    {
        assert(storage_);
        return *storage_;
    }

public:
    field_row_reader(std::vector<field_view>& storage) noexcept : storage_(&storage) {}
    void clear() noexcept override { get().clear(); }
    error_code meta_check(metadata_collection_view, diagnostics&) override { return error_code(); }
    bool has_space() const noexcept override { return true; }
    field_view* add_row(std::size_t num_columns) override { return add_fields(get(), num_columns); }
    error_code parse_row() noexcept override { return error_code(); }
};

template <class RowType, std::size_t N>
class array_row_reader : public row_reader
{
    std::array<RowType, N> rows_{};
    std::array<field_view, std::tuple_size<RowType>::value> temp_fields_;
    std::size_t size_{0};

public:
    array_row_reader() = default;
    void clear() noexcept override { size_ = 0; }
    error_code meta_check(metadata_collection_view meta, diagnostics& diag) override
    {
        return do_meta_check<RowType>(meta, diag);
    }
    bool has_space() const noexcept override { return size_ < rows_.size(); }
    field_view* add_row(std::size_t num_columns) override
    {
        assert(has_space());
        assert(num_columns == temp_fields_.size());
        ++size_;
        return temp_fields_.data();
    }
    error_code parse_row() noexcept override { return do_parse_impl(temp_fields_.data(), rows_[size_ - 1]); }
};

class execution_state_impl : public execution_state_iface
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
    row_reader* reader_{nullptr};

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

public:
    execution_state_impl() = default;

    void set_reader(row_reader& reader) noexcept { reader_ = &reader; }

    void reset(resultset_encoding enc) noexcept override
    {
        reset_base(enc);
        remaining_meta_ = 0;
        reader_ = nullptr;
        clear_previous_resultset();
    }

    error_code on_num_meta(std::size_t num_columns) override
    {
        assert(should_read_head());
        clear_previous_resultset();
        remaining_meta_ = num_columns;
        meta_.reserve(num_columns);
        set_state(state_t::reading_metadata);
        return error_code();
    }

    void on_meta(const column_definition_packet& pack, metadata_mode mode) override
    {
        assert(should_read_meta());
        meta_.push_back(metadata_access::construct(pack, mode == metadata_mode::full));
        if (--remaining_meta_ == 0)
        {
            set_state(state_t::reading_rows);
        }
    }

    error_code meta_check(diagnostics&) const override { return error_code(); }

    void on_row_batch_start() override
    {
        assert(state() == state_t::reading_rows);
        assert(reader_);
        reader_->clear();
    }

    bool can_add_row() const override
    {
        assert(reader_);
        return reader_->has_space();
    }

    field_view* add_row_storage() override
    {
        assert(state() == state_t::reading_rows);
        assert(reader_);
        return reader_->add_row(meta_.size());
    }

    error_code parse_row() override { return reader_->parse_row(); }

    void on_row_batch_finish() override {}

    error_code on_head_ok_packet(const ok_packet& pack) override
    {
        assert(should_read_head());
        clear_previous_resultset();
        on_ok_packet_impl(pack);
        return error_code();
    }

    error_code on_row_ok_packet(const ok_packet& pack) override
    {
        assert(state() == state_t::reading_rows);
        on_ok_packet_impl(pack);
        return error_code();
    }

    metadata_collection_view current_resultset_meta() const noexcept override
    {
        assert(state() == state_t::reading_rows);
        return meta_;
    }

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
};

template <class... RowType>
struct vtable_entry
{
    using tuple_t = std::tuple<std::vector<RowType>...>;
    error_code (*meta_check)(metadata_collection_view, diagnostics&);
    error_code (*parse)(tuple_t&, const field_view*);

    template <std::size_t I>
    static error_code meta_check_fn(metadata_collection_view meta, diagnostics& diag)
    {
        return do_meta_check<typename std::tuple_element<I, tuple_t>::type>(meta, diag);
    }

    template <std::size_t I>
    static error_code parse_fn(tuple_t& to, const field_view* from)
    {
        auto& v = std::get<I>(to);
        v.emplace_back();
        return do_parse_impl(from, v.back());
    }

    template <std::size_t I>
    static vtable_entry create()
    {
        return {&meta_check_fn<I>, &parse_fn<I>};
    }
};

template <class... RowType>
using vtable_t = std::array<vtable_entry<RowType...>, sizeof...(RowType)>;

template <class... RowType, std::size_t... N>
constexpr vtable_t<RowType...> make_vtable_impl(boost::mp11::index_sequence<N...>)
{
    return vtable_t<RowType...>{vtable_entry<RowType...>::template create<N>()...};
}

template <class... RowType>
constexpr vtable_t<RowType...> make_vtable()
{
    return make_vtable_impl<RowType...>(boost::mp11::make_index_sequence<sizeof...(RowType)>());
}

template <class... RowType>
struct get_out_params
{
    using type = void;
};

template <class RowType0, class RowType1, class... Rest>
struct get_out_params<RowType0, RowType1, Rest...>
{
    using type = const typename std::tuple_element<sizeof...(Rest), std::tuple<RowType0, RowType1, Rest...>>::
        type*;
};

struct basic_per_resultset_data
{
    std::size_t meta_offset{};
    std::size_t info_offset{};
    std::size_t info_size{};
    bool has_ok_data{false};         // The OK packet information is default constructed, or actual data?
    std::uint64_t affected_rows{};   // OK packet data
    std::uint64_t last_insert_id{};  // OK packet data
    std::uint16_t warnings{};        // OK packet data
    bool is_out_params{false};       // Does this resultset contain OUT param information?
};

constexpr std::size_t get_max_impl(std::size_t lhs, std::size_t rhs) noexcept
{
    return lhs > rhs ? lhs : rhs;
}

template <std::size_t N>
constexpr std::size_t get_max(const std::size_t (&arr)[N], std::size_t i = 0)
{
    return i >= N ? 0 : get_max_impl(arr[i], get_max(arr, i + 1));
}

template <std::size_t N>
constexpr std::size_t get_sum(const std::size_t (&arr)[N], std::size_t i = 0)
{
    return i >= N ? 0 : arr[i] + get_sum(arr, i + 1);
}

template <class... RowType>
class basic_results_impl : public execution_state_iface
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
        auto& resultset_data = current_resultset();
        std::size_t offset = 0;
        for (std::size_t i = 0; i < resultset_index_; ++i)
            offset += num_columns[i];
        resultset_data.meta_offset = offset;
        resultset_data.info_offset = info_.size();
    }

    void on_ok_packet_impl(const ok_packet& pack)
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
        }
        else
        {
            set_state(state_t::complete);
        }
    }

    const basic_per_resultset_data& current_resultset() const noexcept
    {
        check_resultset_index();
        return per_resultset_[resultset_index_];
    }

    const basic_per_resultset_data& get_resultset_with_ok_packet(std::size_t index) const noexcept
    {
        assert(index < num_resultsets);
        const auto& res = per_resultset_[index];
        assert(res.has_ok_data);
        return res;
    }

public:
    basic_results_impl() = default;

    void reset(resultset_encoding enc) noexcept override
    {
        reset_base(enc);
        meta_index_ = 0;
        boost::mp11::mp_for_each<boost::mp11::mp_iota_c<num_resultsets>>(reset_fn{rows_});
        first_ = true;
        resultset_index_ = 0;
    }

    error_code on_num_meta(std::size_t num_columns) override
    {
        assert(should_read_head());
        if (resultset_index_ >= num_resultsets)
        {
            return client_errc::num_resultsets_mismatch;
        }
        add_resultset();
        if (num_columns != current_num_columns())
        {
            return client_errc::num_columns_mismatch;
        }
        set_state(state_t::reading_metadata);
        return error_code();
    }

    void on_meta(const column_definition_packet& pack, metadata_mode mode) override
    {
        assert(should_read_meta());
        assert(meta_index_ < current_num_columns());
        meta_[current_resultset().meta_offset + meta_index_] = metadata_access::construct(
            pack,
            mode == metadata_mode::full
        );
        if (++meta_index_ == current_num_columns())
        {
            set_state(state_t::reading_rows);
        }
    }

    error_code meta_check(diagnostics& diag) const override
    {
        constexpr auto vtab = make_vtable<RowType...>();
        check_resultset_index();
        return vtab[resultset_index_].meta_check(current_resultset_meta(), diag);
    }

    void on_row_batch_start() override {}

    bool can_add_row() const override { return true; }

    field_view* add_row_storage() override
    {
        assert(state() == state_t::reading_rows);
        return temp_fields_.data();
    }

    error_code parse_row() override
    {
        constexpr auto vtab = make_vtable<RowType...>();
        check_resultset_index();
        return vtab[resultset_index_].parse(rows_, temp_fields_.data());
    }

    void on_row_batch_finish() override {}

    error_code on_head_ok_packet(const ok_packet& pack) override
    {
        assert(should_read_head());
        add_resultset();
        on_ok_packet_impl(pack);
        return error_code();
    }

    error_code on_row_ok_packet(const ok_packet& pack) override
    {
        assert(state() == state_t::reading_rows);
        on_ok_packet_impl(pack);
        return error_code();
    }

    metadata_collection_view current_resultset_meta() const noexcept override
    {
        assert(state() == state_t::reading_rows);
        return get_meta(resultset_index_);
    }

    // User facing
    using out_params_t = typename get_out_params<RowType...>::type;
    out_params_t get_out_params() const noexcept
    {
        static_assert(sizeof...(RowType) > 1, "out_params is only available for multi-resultset operations");
        constexpr std::size_t index = sizeof...(RowType) - 2;

        if (get_is_out_params(index))
        {
            const auto& result = std::get<sizeof...(RowType) - 2>(rows_);
            return result.empty() ? nullptr : &result.front();
        }
        else
        {
            return nullptr;
        }
    }

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
