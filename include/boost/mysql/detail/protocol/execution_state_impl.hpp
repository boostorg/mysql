//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_EXECUTION_STATE_IMPL_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_EXECUTION_STATE_IMPL_HPP

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

#include <boost/container/small_vector.hpp>

namespace boost {
namespace mysql {
namespace detail {

class execution_state_impl
{
public:
    // Append mode=true is used by single-function operations. Metadata and info are
    // appended to the collections here. Append mode=false is used by multi-function
    // operations. Every new resultset wipes the previous one
    execution_state_impl(bool append_mode) noexcept : append_mode_(append_mode) {}

    // State accessors
    bool should_read_head() const noexcept { return state_ == state_t::reading_first_packet; }
    bool should_read_meta() const noexcept { return state_ == state_t::reading_metadata; }
    bool should_read_rows() const noexcept { return state_ == state_t::reading_rows; }
    bool complete() const noexcept { return state_ == state_t::complete; }

    // State transitions
    void reset(detail::resultset_encoding encoding) noexcept
    {
        state_ = state_t::reading_first_packet;
        seqnum_ = 0;
        encoding_ = encoding;
        meta_.clear();
        rows_.clear();
        per_result_.clear();
        info_.clear();
    }

    void on_num_meta(std::size_t num_columns)
    {
        assert(state_ == state_t::reading_first_packet);
        on_new_resultset();
        meta_.reserve(meta_.size() + num_columns);
        auto& resultset_data = current_resultset();
        resultset_data.num_columns = num_columns;
        resultset_data.remaining_meta = num_columns;
        state_ = state_t::reading_metadata;
    }

    void on_meta(const detail::column_definition_packet& pack, metadata_mode mode)
    {
        assert(state_ == state_t::reading_metadata);
        meta_.push_back(metadata_access::construct(pack, mode == metadata_mode::full));
        if (--current_resultset().remaining_meta == 0)
        {
            state_ = state_t::reading_rows;
        }
    }

    void on_row() noexcept
    {
        assert(state_ == state_t::reading_rows);
        ++current_resultset().num_rows;
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
    bool is_append_mode() const noexcept { return append_mode_; }  // For testing

    metadata_collection_view current_resultset_meta() const noexcept
    {
        assert(state_ == state_t::reading_rows);
        return get_meta(per_result_.size() - 1);
    }

    row_impl& rows() noexcept { return rows_; }

    // Accessors for user-facing components
    std::size_t num_resultsets() const noexcept { return per_result_.size(); }

    rows_view get_rows(std::size_t index) const noexcept
    {
        assert(append_mode_);
        const auto& resultset_data = get_resultset(index);
        return rows_view_access::construct(
            rows_.fields().data() + resultset_data.field_offset,
            resultset_data.num_rows * resultset_data.num_columns,
            resultset_data.num_columns
        );
    }

    metadata_collection_view get_meta(std::size_t index) const noexcept
    {
        const auto& resultset_data = get_resultset(index);
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

    struct per_resultset_data
    {
        std::size_t num_columns{};       // Number of columns this resultset has
        std::size_t remaining_meta{};    // State for reading meta op
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
        resultset_data.field_offset = rows_.fields().size();
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
        state_ = pack.status_flags & SERVER_MORE_RESULTS_EXISTS ? state_t::reading_first_packet
                                                                : state_t::complete;
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

    const per_resultset_data& get_resultset(std::size_t index) const noexcept
    {
        assert(index < per_result_.size());
        return per_result_[index];
    }

    const per_resultset_data& get_resultset_with_ok_packet(std::size_t index) const noexcept
    {
        const auto& res = get_resultset(index);
        assert(res.has_ok_packet_data);
        return res;
    }

    bool append_mode_{false};
    state_t state_{state_t::reading_first_packet};
    std::uint8_t seqnum_{};
    detail::resultset_encoding encoding_{detail::resultset_encoding::text};
    std::vector<metadata> meta_;
    row_impl rows_;
    boost::container::small_vector<per_resultset_data, 1> per_result_;
    std::vector<char> info_;
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
