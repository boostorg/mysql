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
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>

#include <cstddef>

namespace boost {
namespace mysql {
namespace test {

class mock_execution_processor : public detail::execution_processor
{
public:
    using execution_processor::execution_processor;

    struct num_calls_t
    {
        std::size_t reset{};
        std::size_t on_head_ok_packet{};
        std::size_t on_num_meta{};
        std::size_t on_meta{};
        std::size_t on_row{};
        std::size_t on_row_ok_packet{};
    } num_calls;

    struct actions_t
    {
        std::function<error_code(const detail::ok_packet&, diagnostics&)> on_head_ok_packet;
        std::function<void(std::size_t)> on_num_meta;
        std::function<error_code(metadata&&, string_view, bool, diagnostics&)> on_meta;
        std::function<error_code(detail::deserialization_context, const detail::output_ref&)> on_row;
        std::function<error_code(const detail::ok_packet&)> on_row_ok_packet;
    } actions;

private:
    void reset_impl() noexcept override { ++num_calls.reset; }
    error_code on_head_ok_packet_impl(const detail::ok_packet& pack, diagnostics& diag) override
    {
        ++num_calls.on_head_ok_packet;
        if (actions.on_head_ok_packet)
            return actions.on_head_ok_packet(pack, diag);
        return error_code();
    }
    void on_num_meta_impl(std::size_t num_columns) override
    {
        ++num_calls.on_num_meta;
        if (actions.on_num_meta)
            actions.on_num_meta(num_columns);
    }
    error_code on_meta_impl(metadata&& m, string_view column_name, bool is_last, diagnostics& diag) override
    {
        ++num_calls.on_meta;
        if (actions.on_meta)
            return actions.on_meta(std::move(m), column_name, is_last, diag);
        return error_code();
    }
    void on_row_batch_start_impl() override {}
    void on_row_batch_finish_impl() override {}
    error_code on_row_impl(detail::deserialization_context ctx, const detail::output_ref& ref) override
    {
        ++num_calls.on_row;
        if (actions.on_row)
            return actions.on_row(ctx, ref);
        return error_code();
    }
    error_code on_row_ok_packet_impl(const detail::ok_packet& pack) override
    {
        ++num_calls.on_row_ok_packet;
        if (actions.on_row_ok_packet)
            return actions.on_row_ok_packet(pack);
        return error_code();
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
