//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PROTOCOL_EXECUTION_PROCESSOR_HPP
#define BOOST_MYSQL_DETAIL_PROTOCOL_EXECUTION_PROCESSOR_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/db_flavor.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

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
        seqnum_ = 0;
        mode_ = mode;
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

    error_code on_row_ok_packet(const ok_packet& pack)
    {
        assert(should_read_rows());
        return on_row_ok_packet_impl(pack);
    }

    error_code on_row(deserialization_context& ctx)
    {
        assert(should_read_rows());
        return on_row_impl(ctx);
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

private:
    state_t state_;
    resultset_encoding encoding_{resultset_encoding::text};
    std::uint8_t seqnum_{};
    metadata_mode mode_{metadata_mode::minimal};
};

class results_base : public execution_processor
{
public:
    virtual void on_row_batch_start() = 0;
    virtual void on_row_batch_finish() = 0;
};

struct output_ref
{
    void* data{};
    std::size_t max_size{};
    std::size_t resultset_number{};

    bool has_value() const noexcept { return data != nullptr; }
};

class execution_state_base : public execution_processor
{
public:
    virtual bool has_space() const = 0;
};

class typed_execution_state_base : public execution_state_base
{
public:
    virtual error_code set_output(output_ref ref) = 0;
    virtual std::size_t num_read_rows() const noexcept = 0;
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
