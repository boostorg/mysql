//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_MOCK_EXECUTION_PROCESSOR_HPP
#define BOOST_MYSQL_TEST_COMMON_MOCK_EXECUTION_PROCESSOR_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/impl/deserialize_row.ipp>

#include <cstddef>

#include "test_stream.hpp"

namespace boost {
namespace mysql {
namespace test {

class mock_execution_processor : public detail::execution_processor
{
public:
    explicit mock_execution_processor(fail_count fc = fail_count(), diagnostics diag = diagnostics())
        : fc_(fc), diag_(std::move(diag))
    {
    }

    struct num_calls_t
    {
        std::size_t reset{};
        std::size_t on_num_meta{};
        std::size_t on_meta{};
        std::size_t on_head_ok_packet{};
        std::size_t on_row_batch_start{};
        std::size_t on_row_batch_finish{};
        std::size_t on_row{};
        std::size_t on_row_ok_packet{};
    };

    const num_calls_t& num_calls() const noexcept { return num_calls_; }
    std::uint64_t affected_rows() const noexcept { return ok_packet_.affected_rows; }
    std::uint64_t last_insert_id() const noexcept { return ok_packet_.last_insert_id; }
    string_view info() const noexcept { return ok_packet_.info; }
    std::size_t num_meta() const noexcept { return num_meta_; }
    const std::vector<metadata>& meta() const noexcept { return meta_; }

private:
    // Data
    num_calls_t num_calls_;
    struct
    {
        std::uint64_t affected_rows{};
        std::uint64_t last_insert_id{};
        std::string info;
    } ok_packet_{};
    std::size_t num_meta_{};
    std::vector<metadata> meta_;
    std::vector<row> rows_;
    fail_count fc_;
    diagnostics diag_;

    // Helpers
    error_code maybe_fail(diagnostics& diag)
    {
        auto err = fc_.maybe_fail();
        if (err)
        {
            diag = diag_;
        }
        return err;
    }

    void handle_ok(const detail::ok_packet& pack)
    {
        ok_packet_.affected_rows = pack.affected_rows.value;
        ok_packet_.last_insert_id = pack.last_insert_id.value;
        ok_packet_.info = pack.info.value;
    }

protected:
    void reset_impl() noexcept override { ++num_calls_.reset; }
    error_code on_head_ok_packet_impl(const detail::ok_packet& pack, diagnostics& diag) override
    {
        ++num_calls_.on_head_ok_packet;
        handle_ok(pack);
        return maybe_fail(diag);
    }
    void on_num_meta_impl(std::size_t) override { ++num_calls_.on_num_meta; }
    error_code on_meta_impl(metadata&& m, string_view, bool is_last, diagnostics& diag) override
    {
        ++num_calls_.on_meta;
        meta_.push_back(std::move(m));
        return is_last ? maybe_fail(diag) : error_code();
    }
    void on_row_batch_start_impl() override { ++num_calls_.on_row_batch_start; }
    void on_row_batch_finish_impl() override { ++num_calls_.on_row_batch_finish; }
    error_code on_row_impl(detail::deserialization_context, const detail::output_ref&, std::vector<field_view>&)
        override
    {
        ++num_calls_.on_row;
        return fc_.maybe_fail();
    }
    error_code on_row_ok_packet_impl(const detail::ok_packet& pack) override
    {
        ++num_calls_.on_row_ok_packet;
        handle_ok(pack);
        return fc_.maybe_fail();
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
