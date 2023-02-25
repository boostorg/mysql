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

#include <boost/mysql/detail/auxiliar/row_base.hpp>
#include <boost/mysql/detail/auxiliar/string_view_offset.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/impl/rows_view.hpp>

#include <boost/container/small_vector.hpp>

namespace boost {
namespace mysql {
namespace detail {

struct execution_state_impl
{
    struct per_resultset_data
    {
        std::size_t num_columns{};
        std::size_t remaining_meta{};
        std::size_t num_rows{};
        bool eof_received{false};
        std::uint64_t affected_rows{};
        std::uint64_t last_insert_id{};
        std::uint16_t warnings{};
        string_view_offset info_offset{};
        bool more_results_exist{};

        bool is_reading_meta() const noexcept { return remaining_meta > 0; }
        bool is_reading_rows() const noexcept { return remaining_meta == 0 && !eof_received; }

        void on_ok_packet(const ok_packet& pack, std::size_t current_info_size) noexcept
        {
            assert(remaining_meta == 0);
            eof_received = true;
            affected_rows = pack.affected_rows.value;
            last_insert_id = pack.last_insert_id.value;
            warnings = pack.warnings;
            info_offset = string_view_offset(current_info_size, pack.info.value.size());
            more_results_exist = pack.status_flags & SERVER_MORE_RESULTS_EXISTS;
        }

        per_resultset_data(std::size_t num_columns) noexcept
            : num_columns(num_columns), remaining_meta(num_columns)
        {
        }
    };

    bool append_mode_{false};
    std::uint8_t seqnum_{};
    detail::resultset_encoding encoding_{detail::resultset_encoding::text};
    std::vector<metadata> meta_;
    row_base rows_;
    boost::container::small_vector<per_resultset_data, 1> per_result_;
    std::vector<char> info_;

    bool has_remaining_meta() const noexcept
    {
        if (per_result_.empty())
            return false;
        return per_result_.back().remaining_meta > 0;
    }

    bool should_read_head() const noexcept
    {
        // We haven't read anything yet, or the last resultset has been completely
        // read and had the MORE_RESULTS flag
        return per_result_.empty() || per_result_.back().is_reading_meta() ||
               (per_result_.back().eof_received && per_result_.back().more_results_exist);
    }

    bool should_read_rows() const noexcept
    {
        return !per_result_.empty() && per_result_.back().is_reading_rows();
    }

    bool complete() const noexcept
    {
        return !per_result_.empty() && per_result_.back().eof_received &&
               !per_result_.back().more_results_exist;
    }

    void reset(detail::resultset_encoding encoding, bool append_mode) noexcept
    {
        append_mode_ = append_mode;
        seqnum_ = 0;
        encoding_ = encoding;
        meta_.clear();
        rows_.clear();
        per_result_.clear();
        info_.clear();
    }

    void on_num_meta(std::size_t num_fields)
    {
        assert(should_read_head());
        if (!append_mode_)
        {
            meta_.clear();
            per_result_.clear();
            info_.clear();
        }
        meta_.reserve(meta_.size() + num_fields);
        per_result_.emplace_back(num_fields);
    }

    void on_meta(const detail::column_definition_packet& pack, metadata_mode mode)
    {
        assert(!per_result_.empty() && per_result_.back().is_reading_meta());
        meta_.push_back(metadata_access::construct(pack, mode == metadata_mode::full));
        --per_result_.back().remaining_meta;
    }

    void on_row() noexcept { per_result_.back().num_rows++; }

    void on_ok_packet(const detail::ok_packet& pack)
    {
        if (per_result_.back().is_reading_rows())
        {
            per_result_.back().on_ok_packet(pack, info_.size());
        }
        else if (per_result_.empty() || per_result_.back().eof_received)
        {
            per_result_.emplace_back(0);
            per_result_.back().on_ok_packet(pack, info_.size());
        }
        else
        {
            assert(false);
        }
        info_.insert(info_.end(), pack.info.value.begin(), pack.info.value.end());
    }

    resultset_encoding encoding() const noexcept { return encoding_; }
    std::uint8_t& sequence_number() noexcept { return seqnum_; }

    metadata_collection_view current_resultset_meta() const noexcept
    {
        assert(!per_result_.empty() && per_result_.back().remaining_meta == 0);
        std::size_t num_metas = per_result_.back().num_columns;
        return metadata_collection_view(meta_.data() + meta_.size() - num_metas, num_metas);
    }

    rows_view get_rows() const noexcept
    {
        return rows_view_access::construct(
            rows_.fields_.data(),
            per_result_.front().num_rows * per_result_.front().num_columns,
            per_result_.front().num_columns
        );
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
