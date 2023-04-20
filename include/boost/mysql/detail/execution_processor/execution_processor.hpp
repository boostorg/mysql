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
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/db_flavor.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

class execution_processor
{
public:
    virtual ~execution_processor() {}

    void reset(resultset_encoding enc, metadata_mode mode) noexcept
    {
        state_ = state_t::initial;
        encoding_ = enc;
        mode_ = mode;
        seqnum_ = 0;
        read_rows_ = 0;
        reset_impl();
    }

    error_code on_head_ok_packet(const ok_packet& pack)
    {
        assert(should_read_head());
        return on_head_ok_packet_impl(pack);
    }

    error_code on_num_meta(std::size_t num_columns)
    {
        assert(should_read_head());
        return on_num_meta_impl(num_columns);
    }

    error_code on_meta(const column_definition_packet& pack, diagnostics& diag)
    {
        assert(should_read_meta());
        return on_meta_impl(pack, diag);
    }

    void on_row_batch_start()
    {
        assert(should_read_rows());
        read_rows_ = 0;
        on_row_batch_start_impl();
    }

    void on_row_batch_finish() { on_row_batch_finish_impl(); }

    error_code on_row_ok_packet(const ok_packet& pack)
    {
        assert(should_read_rows());
        return on_row_ok_packet_impl(pack);
    }

    error_code on_row(deserialization_context& ctx)
    {
        assert(should_read_rows());
        error_code ec = on_row_impl(ctx);
        if (ec)
            return ec;
        ++read_rows_;
        return error_code();
    }

    bool initial() const noexcept { return state_ == state_t::initial; }
    bool should_read_head() const noexcept
    {
        return state_ == state_t::initial || state_ == state_t::reading_first_packet;
    }
    bool should_read_head_subsequent() const noexcept { return state_ == state_t::reading_first_packet; }
    bool should_read_meta() const noexcept { return state_ == state_t::reading_metadata; }
    bool should_read_rows() const noexcept { return state_ == state_t::reading_rows; }
    bool complete() const noexcept { return state_ == state_t::complete; }

    std::size_t num_read_rows() const noexcept { return read_rows_; }
    std::size_t num_meta() const noexcept { return num_meta_impl(); }

    resultset_encoding encoding() const noexcept { return encoding_; }
    std::uint8_t& sequence_number() noexcept { return seqnum_; }

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

    metadata_mode meta_mode() const noexcept { return mode_; }

    virtual void reset_impl() noexcept = 0;
    virtual error_code on_head_ok_packet_impl(const ok_packet& pack) = 0;
    virtual error_code on_num_meta_impl(std::size_t num_columns) = 0;
    virtual error_code on_meta_impl(const column_definition_packet& pack, diagnostics& diag) = 0;
    virtual error_code on_row_ok_packet_impl(const ok_packet& pack) = 0;
    virtual error_code on_row_impl(deserialization_context& ctx) = 0;
    virtual void on_row_batch_start_impl() = 0;
    virtual void on_row_batch_finish_impl() = 0;
    virtual std::size_t num_meta_impl() const noexcept = 0;

private:
    state_t state_;
    resultset_encoding encoding_{resultset_encoding::text};
    std::uint8_t seqnum_{};
    metadata_mode mode_{metadata_mode::minimal};
    std::size_t read_rows_{};
};

// A type-erased reference to be used as the output range for execution states.
// For static_execution_state, this is a span-like class.
// For execution_state, this holds a pointer to the field vector to use
struct output_ref
{
    // Pointer to the first element of the span (static_execution_state)
    // Pointer to a vector<field_view> to use as output (execution_state)
    void* data{};

    // Number of elements in the span (static_execution_state). Otherwise unused
    std::size_t max_size{};

    // Identifier for the type of elements. Index in the resultset type list (static_execution_state).
    // Otherwise unused
    std::size_t resultset_index{};

    constexpr output_ref() noexcept = default;

    constexpr output_ref(void* data, std::size_t max_size, std::size_t resultset_index) noexcept
        : data(data), max_size(max_size), resultset_index(resultset_index)
    {
    }

    constexpr output_ref(void* data) noexcept : data(data) {}

    bool has_value() const noexcept { return data != nullptr; }
};

class execution_processor_with_output : public execution_processor
{
    output_ref output_;

public:
    void set_output(const output_ref& output) noexcept { output_ = output; }
    const output_ref& output() const noexcept { return output_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
