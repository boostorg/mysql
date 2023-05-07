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
#include <boost/mysql/detail/execution_processor/static_execution_state_impl.hpp>
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

    detail::execution_processor& iface() noexcept { return res_.get_interface(); }

public:
    basic_exec_builder() = default;

    basic_exec_builder& reset(
        detail::resultset_encoding enc = detail::resultset_encoding::text,
        metadata_mode mode = metadata_mode::minimal
    )
    {
        iface().reset(enc, mode);
        return *this;
    }
    basic_exec_builder& seqnum(std::uint8_t v)
    {
        iface().sequence_number() = v;
        return *this;
    }
    basic_exec_builder& meta(const std::vector<detail::protocol_field_type>& types)
    {
        diagnostics diag;
        iface().on_num_meta(types.size());
        for (auto type : types)
        {
            auto err = iface().on_meta(create_coldef(type), diag);
            throw_on_error(err, diag);
        }
        return *this;
    }
    basic_exec_builder& meta(std::vector<metadata> meta)
    {
        add_meta(iface(), std::move(meta));
        return *this;
    }

    basic_exec_builder& ok(const detail::ok_packet& pack)
    {
        add_ok(iface(), pack);
        return *this;
    }

    T build() { return std::move(res_); }
};

using exec_builder = basic_exec_builder<detail::execution_state_impl>;

template <class... StaticRow>
using static_exec_builder = basic_exec_builder<detail::static_execution_state_impl<StaticRow...>>;

struct resultset_spec
{
    std::vector<detail::protocol_field_type> types;
    boost::mysql::rows r;
    detail::ok_packet ok;

    bool empty() const noexcept { return types.empty(); }
};

// inline results create_results(const std::vector<resultset_spec>& spec)
// {
//     exec_builder builder;
//     for (std::size_t i = 0; i < spec.size(); ++i)
//     {
//         const auto& speci = spec[i];

//         // Meta
//         if (!spec.empty())
//         {
//             builder.meta(speci.types);
//         }

//         // Rows
//         if (!speci.r.empty())
//         {
//             builder.rows(speci.r);
//         }

//         // OK packate
//         bool is_last = i == spec.size() - 1;
//         detail::ok_packet actual_ok = speci.ok;
//         if (!is_last)
//         {
//             actual_ok.status_flags |= detail::SERVER_MORE_RESULTS_EXISTS;
//         }
//         builder.ok(actual_ok);
//     }
//     // TODO
// }

inline detail::execution_state_impl& get_impl(execution_state& st)
{
    return detail::impl_access::get_impl(st);
}

inline detail::results_impl& get_impl(results& r) { return detail::impl_access::get_impl(r); }

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
