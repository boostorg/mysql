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
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/rows.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/execution_processor/execution_state_impl.hpp>
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

// execution_state_impl
class exec_builder
{
    detail::execution_state_impl res_;

public:
    exec_builder() = default;

    exec_builder& reset(
        detail::resultset_encoding enc = detail::resultset_encoding::text,
        metadata_mode mode = metadata_mode::minimal
    )
    {
        res_.reset(enc, mode);
        return *this;
    }
    exec_builder& seqnum(std::uint8_t v)
    {
        res_.sequence_number() = v;
        return *this;
    }
    exec_builder& meta(const std::vector<detail::protocol_field_type>& types)
    {
        diagnostics diag;
        res_.on_num_meta(types.size());
        for (auto type : types)
        {
            auto err = res_.on_meta(create_coldef(type), diag);
            throw_on_error(err, diag);
        }
        return *this;
    }
    // exec_builder& rows(const boost::mysql::rows& r)
    // {
    //     assert(res_.encoding() == detail::resultset_encoding::text);
    //     assert(r.num_columns() == res_.meta().size());
    //     res_.on_row_batch_start();
    //     for (auto rv : r)
    //     {
    //         // TODO: this is wrong
    //         auto s = create_text_row_body_span(boost::span<const field_view>(rv.begin(), rv.size()));
    //         detail::deserialization_context ctx{s.data(), s.data() + s.size(), detail::capabilities()};
    //         throw_on_error(res_.on_row(ctx, detail::output_ref()));
    //     }
    //     res_.on_row_batch_finish();
    //     return *this;
    // }

    exec_builder& ok(const detail::ok_packet& pack)
    {
        diagnostics diag;
        error_code err;
        if (res_.is_reading_head())
            err = res_.on_head_ok_packet(pack, diag);
        else
            err = res_.on_row_ok_packet(pack);
        throw_on_error(err, diag);
        return *this;
    }

    detail::execution_state_impl build() { return std::move(res_); }
};

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
