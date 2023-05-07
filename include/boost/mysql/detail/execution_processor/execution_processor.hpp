//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_EXECUTION_PROCESSOR_HPP
#define BOOST_MYSQL_DETAIL_EXECUTION_PROCESSOR_EXECUTION_PROCESSOR_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/db_flavor.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <boost/config.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

// A type-erased reference to be used as the output range for execution states.
// For static_execution_state, this is a span-like class.
// For execution_state, this holds a pointer to the field vector to use
class output_ref
{
    // Pointer to the first element of the span (static_execution_state)
    // Pointer to a vector<field_view> to use as output (execution_state)
    void* data_{};

    // Number of elements in the span (static_execution_state). Otherwise unused
    std::size_t max_size_{};

    // Identifier for the type of elements. Index in the resultset type list (static_execution_state).
    // type_index_none otherwise
    std::size_t type_index_{type_index_none};

    // Offset into the span's data (static_execution_state). Otherwise unused
    std::size_t offset_{};

    static constexpr std::size_t type_index_none = static_cast<std::size_t>(-1);

public:
    constexpr output_ref() noexcept = default;

    template <class T>
    constexpr output_ref(boost::span<T> span, std::size_t type_index, std::size_t offset = 0) noexcept
        : data_(span.data()), max_size_(span.size()), type_index_(type_index), offset_(offset)
    {
    }

    output_ref(std::vector<field_view>& storage) noexcept : data_(&storage) {}

    std::size_t max_size() const noexcept { return max_size_; }
    std::size_t type_index() const noexcept { return type_index_; }
    std::size_t offset() const noexcept { return offset_; }
    void inc_offset() noexcept { ++offset_; }

    template <class T>
    T& span_element() const noexcept
    {
        assert(data_);
        assert(type_index_ != type_index_none);
        return static_cast<T*>(data_)[offset_];
    }

    std::vector<field_view>& fields() const noexcept
    {
        assert(data_);
        assert(type_index_ == type_index_none);
        return *static_cast<std::vector<field_view>*>(data_);
    }
};

class execution_processor
{
public:
    virtual ~execution_processor() {}

    void reset(resultset_encoding enc, metadata_mode mode) noexcept
    {
        state_ = state_t::reading_first;
        encoding_ = enc;
        mode_ = mode;
        seqnum_ = 0;
        reset_impl();
    }

    BOOST_ATTRIBUTE_NODISCARD error_code on_head_ok_packet(const ok_packet& pack, diagnostics& diag)
    {
        assert(is_reading_head());
        return on_head_ok_packet_impl(pack, diag);
    }

    void on_num_meta(std::size_t num_columns)
    {
        assert(is_reading_head());
        on_num_meta_impl(num_columns);
    }

    BOOST_ATTRIBUTE_NODISCARD error_code on_meta(const column_definition_packet& pack, diagnostics& diag)
    {
        assert(is_reading_meta());
        return on_meta_impl(
            metadata_access::construct(pack, meta_mode() == metadata_mode::full),
            pack.name.value,
            diag
        );
    }

    // Exposed for the sake of testing
    BOOST_ATTRIBUTE_NODISCARD error_code on_meta(metadata&& meta, diagnostics& diag)
    {
        assert(is_reading_meta());
        return on_meta_impl(std::move(meta), meta.column_name(), diag);
    }

    void on_row_batch_start()
    {
        assert(is_reading_rows());
        on_row_batch_start_impl();
    }

    void on_row_batch_finish() { on_row_batch_finish_impl(); }

    BOOST_ATTRIBUTE_NODISCARD error_code on_row_ok_packet(const ok_packet& pack)
    {
        assert(is_reading_rows());
        return on_row_ok_packet_impl(pack);
    }

    BOOST_ATTRIBUTE_NODISCARD error_code on_row(deserialization_context ctx, const output_ref& ref)
    {
        assert(is_reading_rows());
        return on_row_impl(ctx, ref);
    }

    bool is_reading_first() const noexcept { return state_ == state_t::reading_first; }
    bool is_reading_first_subseq() const noexcept { return state_ == state_t::reading_first_subseq; }
    bool is_reading_head() const noexcept
    {
        return state_ == state_t::reading_first || state_ == state_t::reading_first_subseq;
    }
    bool is_reading_meta() const noexcept { return state_ == state_t::reading_metadata; }
    bool is_reading_rows() const noexcept { return state_ == state_t::reading_rows; }
    bool is_complete() const noexcept { return state_ == state_t::complete; }

    resultset_encoding encoding() const noexcept { return encoding_; }
    std::uint8_t& sequence_number() noexcept { return seqnum_; }

protected:
    enum class state_t
    {
        // waiting for 1st packet, for the 1st resultset
        reading_first,

        // same, but for subsequent resultsets (distiguised to provide a cleaner xp to
        // the user in (static_)execution_state)
        reading_first_subseq,

        // waiting for metadata packets
        reading_metadata,

        // waiting for rows
        reading_rows,

        // done
        complete
    };
    void set_state(state_t v) noexcept { state_ = v; }

    metadata_mode meta_mode() const noexcept { return mode_; }

    virtual void reset_impl() noexcept = 0;
    virtual error_code on_head_ok_packet_impl(const ok_packet& pack, diagnostics& diag) = 0;
    virtual void on_num_meta_impl(std::size_t num_columns) = 0;
    virtual error_code on_meta_impl(metadata&& meta, string_view column_name, diagnostics& diag) = 0;
    virtual error_code on_row_ok_packet_impl(const ok_packet& pack) = 0;
    virtual error_code on_row_impl(deserialization_context ctx, const output_ref& ref) = 0;
    virtual void on_row_batch_start_impl() = 0;
    virtual void on_row_batch_finish_impl() = 0;

private:
    state_t state_{state_t::reading_first};
    resultset_encoding encoding_{resultset_encoding::text};
    std::uint8_t seqnum_{};
    metadata_mode mode_{metadata_mode::minimal};
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
