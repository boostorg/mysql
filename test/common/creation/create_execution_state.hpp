//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_EXECUTION_STATE_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_EXECUTION_STATE_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>
#include <boost/mysql/detail/execution_processor/results_impl.hpp>
#include <boost/mysql/detail/execution_processor/static_execution_state_impl.hpp>
#include <boost/mysql/detail/execution_processor/static_results_impl.hpp>
#include <boost/mysql/detail/protocol/capabilities.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <cstddef>

#include "creation/create_message_struct.hpp"
#include "creation/create_row_message.hpp"
#include "test_common.hpp"

namespace boost {
namespace mysql {
namespace test {

inline void add_meta(detail::execution_processor& proc, std::vector<metadata> meta)
{
    diagnostics diag;
    proc.on_num_meta(meta.size());
    for (auto& m : meta)
    {
        auto err = proc.on_meta(std::move(m), diag);
        throw_on_error(err, diag);
    }
}

inline void add_meta(detail::execution_processor& proc, const std::vector<detail::protocol_field_type>& types)
{
    diagnostics diag;
    proc.on_num_meta(types.size());
    for (auto type : types)
    {
        auto err = proc.on_meta(create_coldef(type), diag);
        throw_on_error(err, diag);
    }
}

// This is only applicable for results types (not for execution_state types)
template <class... T>
void add_row(detail::execution_processor& proc, const T&... args)
{
    rowbuff buff{args...};
    proc.on_row_batch_start();
    auto err = proc.on_row(buff.ctx(), detail::output_ref());
    throw_on_error(err);
    proc.on_row_batch_finish();
}

inline void add_ok(detail::execution_processor& proc, const detail::ok_packet& pack)
{
    diagnostics diag;
    error_code err;
    if (proc.is_reading_head())
        err = proc.on_head_ok_packet(pack, diag);
    else
        err = proc.on_row_ok_packet(pack);
    throw_on_error(err, diag);
}

template <class T>
class basic_exec_builder
{
    T res_;

public:
    basic_exec_builder() = default;

    basic_exec_builder& reset(
        detail::resultset_encoding enc = detail::resultset_encoding::text,
        metadata_mode mode = metadata_mode::minimal
    )
    {
        res_.get_interface().reset(enc, mode);
        return *this;
    }
    basic_exec_builder& meta(const std::vector<detail::protocol_field_type>& types)
    {
        add_meta(res_.get_interface(), types);
        return *this;
    }
    basic_exec_builder& meta(std::vector<metadata> meta)
    {
        add_meta(res_.get_interface(), std::move(meta));
        return *this;
    }
    template <class... Args>
    basic_exec_builder& row(const Args&... args)
    {
        add_row(res_.get_interface(), args...);
        return *this;
    }
    basic_exec_builder& ok(const detail::ok_packet& pack)
    {
        add_ok(res_.get_interface(), pack);
        return *this;
    }
    T build() { return std::move(res_); }
};

using exec_builder = basic_exec_builder<detail::execution_state_impl>;
using results_builder = basic_exec_builder<detail::results_impl>;

template <class... StaticRow>
using static_exec_builder = basic_exec_builder<detail::static_execution_state_impl<StaticRow...>>;

template <class... StaticRow>
using static_results_builder = basic_exec_builder<detail::static_results_impl<StaticRow...>>;

// inline detail::execution_state_impl& get_impl(execution_state& st)
// {
//     return detail::impl_access::get_impl(st);
// }

// inline detail::results_impl& get_impl(results& r) { return detail::impl_access::get_impl(r); }

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
