//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_EXECUTION_STATE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_EXECUTION_STATE_IMPL_HPP

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/rows_view.hpp>

#include <boost/mysql/detail/auxiliar/row_impl.hpp>
#include <boost/mysql/detail/auxiliar/string_view_offset.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/impl/rows_view.hpp>

#include <cassert>
#include <cstddef>

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
            return rest_.emplace_back();
        }
    }
};

class execution_state_impl
{
public:
    // Append mode=true is used by single-function operations. Metadata and info are
    // appended to the collections here. Append mode=false is used by multi-function
    // operations. Every new resultset wipes the previous one
    // In append mode, rows are owned by this class. In non-append mode,
    // they're owned by the external storage (usually the channel).
    execution_state_impl(bool append_mode) noexcept : append_mode_(append_mode) {}

    bool is_append_mode() const noexcept { return append_mode_; }

    // State accessors
    bool should_read_head() const noexcept { return state_ == state_t::reading_first_packet; }
    bool should_read_meta() const noexcept { return state_ == state_t::reading_metadata; }
    bool should_read_rows() const noexcept { return state_ == state_t::reading_rows; }
    bool complete() const noexcept { return state_ == state_t::complete; }

    // State transitions
    void reset(detail::resultset_encoding encoding, std::vector<field_view>* field_storage) noexcept
    {
        state_ = state_t::reading_first_packet;
        seqnum_ = 0;
        encoding_ = encoding;
        remaining_meta_ = 0;
        meta_.clear();
        per_result_.clear();
        info_.clear();
        if (append_mode_)
        {
            rows_.reset();
        }
        else
        {
            assert(field_storage != nullptr);
            external_rows_ = field_storage;
        }
    }

    void on_num_meta(std::size_t num_columns)
    {
        assert(state_ == state_t::reading_first_packet);
        on_new_resultset();
        meta_.reserve(meta_.size() + num_columns);
        auto& resultset_data = current_resultset();
        resultset_data.num_columns = num_columns;
        remaining_meta_ = num_columns;
        state_ = state_t::reading_metadata;
    }

    void on_meta(const detail::column_definition_packet& pack, metadata_mode mode)
    {
        assert(state_ == state_t::reading_metadata);
        meta_.push_back(metadata_access::construct(pack, mode == metadata_mode::full));
        if (--remaining_meta_ == 0)
        {
            state_ = state_t::reading_rows;
        }
    }

    void on_row_batch_start() noexcept
    {
        assert(state_ == state_t::reading_rows);
        if (append_mode_)
        {
            rows_.start_batch();
        }
        else
        {
            external_rows_->clear();
        }
    }

    field_view* add_row()
    {
        assert(state_ == state_t::reading_rows);
        std::size_t num_fields = current_resultset().num_columns;
        field_view* res = append_mode_ ? rows_.add_row(num_fields) : add_fields(*external_rows_, num_fields);
        ++current_resultset().num_rows;
        return res;
    }

    void on_row_batch_finish()
    {
        // it's legal to invoke this function from any state
        if (append_mode_)
        {
            rows_.finish_batch();
        }
    }

    // When we receive an OK packet in an execute function, as the first packet
    void on_head_ok_packet(const ok_packet& pack)
    {
        assert(state_ == state_t::reading_first_packet);
        on_new_resultset();
        on_ok_packet_impl(pack);
    }

    // When we receive an OK packet while reading rows
    void on_row_ok_packet(const ok_packet& pack)
    {
        assert(state_ == state_t::reading_rows);
        on_ok_packet_impl(pack);
    }

    // Accessors for other protocol stuff
    resultset_encoding encoding() const noexcept { return encoding_; }
    std::uint8_t& sequence_number() noexcept { return seqnum_; }

    metadata_collection_view current_resultset_meta() const noexcept
    {
        assert(state_ == state_t::reading_rows);
        return get_meta(per_result_.size() - 1);
    }

    // Accessors for user-facing components
    std::size_t num_resultsets() const noexcept { return per_result_.size(); }

    rows_view get_rows(std::size_t index) const noexcept
    {
        assert(append_mode_);
        const auto& resultset_data = per_result_[index];
        return rows_
            .rows_slice(resultset_data.field_offset, resultset_data.num_columns, resultset_data.num_rows);
    }

    rows_view get_external_rows() const noexcept
    {
        assert(!append_mode_);
        return rows_view_access::construct(
            external_rows_->data(),
            external_rows_->size(),
            current_resultset().num_columns
        );
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

    row_view get_out_params() const noexcept
    {
        assert(append_mode_ && state_ == state_t::complete);
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

private:
    enum class state_t
    {
        reading_first_packet,  // we're waiting for a resultset's 1st packet
        reading_metadata,
        reading_rows,
        complete
    };

    void on_new_resultset() noexcept
    {
        // Clean stuff from previous resultsets if we're not in append mode
        if (!append_mode_)
        {
            meta_.clear();
            per_result_.clear();
            info_.clear();
        }

        // Allocate a new per-resultset object
        auto& resultset_data = per_result_.emplace_back();
        resultset_data.meta_offset = meta_.size();
        resultset_data.field_offset = rows_.num_fields();
        resultset_data.info_offset = info_.size();
    }

    void on_ok_packet_impl(const ok_packet& pack) noexcept
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
            state_ = state_t::reading_first_packet;
        }
        else
        {
            if (append_mode_)
            {
                rows_.finish();
            }
            state_ = state_t::complete;
        }
    }

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

    const per_resultset_data& get_resultset_with_ok_packet(std::size_t index) const noexcept
    {
        const auto& res = per_result_[index];
        assert(res.has_ok_packet_data);
        return res;
    }

    bool append_mode_{false};
    state_t state_{state_t::reading_first_packet};
    std::uint8_t seqnum_{};
    detail::resultset_encoding encoding_{detail::resultset_encoding::text};
    std::size_t remaining_meta_{};
    std::vector<metadata> meta_;
    resultset_container per_result_;
    std::vector<char> info_;
    multi_rows rows_;                           // if append_mode
    std::vector<field_view>* external_rows_{};  // if !append_mode
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
