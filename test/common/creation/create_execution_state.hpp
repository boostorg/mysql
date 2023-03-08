//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_EXECUTION_STATE_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_EXECUTION_STATE_HPP

#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/rows.hpp>

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/detail/protocol/execution_state_impl.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <cstddef>

#include "creation/create_message_struct.hpp"

namespace boost {
namespace mysql {
namespace test {

struct resultset_spec
{
    std::vector<detail::protocol_field_type> types;
    boost::mysql::rows r;
    detail::ok_packet ok;

    bool empty() const noexcept { return types.empty(); }
};

// execution_state_impl
class exec_builder
{
    detail::execution_state_impl res_;

public:
    exec_builder(bool append_mode, detail::resultset_encoding enc = detail::resultset_encoding::text)
        : res_(append_mode)
    {
        res_.reset(enc);
    }

    exec_builder& seqnum(std::uint8_t v)
    {
        res_.sequence_number() = v;
        return *this;
    }

    exec_builder& meta(const std::vector<detail::protocol_field_type>& types)
    {
        res_.on_num_meta(types.size());
        for (auto type : types)
        {
            res_.on_meta(create_coldef(type), metadata_mode::minimal);
        }
        return *this;
    }

    exec_builder& row()
    {
        res_.on_row();
        return *this;
    }

    exec_builder& rows(const boost::mysql::rows& r)
    {
        assert(r.num_columns() == res_.current_resultset_meta().size());
        auto num_fields = r.size() * r.num_columns();
        auto* storage = res_.rows().add_fields(num_fields);
        for (auto rv : r)
        {
            for (auto f : rv)
            {
                *(storage++) = f;
            }
            res_.on_row();
        }
        res_.rows().copy_strings_as_offsets(0, num_fields);
        return *this;
    }

    exec_builder& ok(const detail::ok_packet& pack)
    {
        if (res_.should_read_head())
            res_.on_head_ok_packet(pack);
        else
            res_.on_row_ok_packet(pack);
        return *this;
    }

    exec_builder& resultset(const resultset_spec& spec, bool is_last = false)
    {
        detail::ok_packet actual_ok{spec.ok};
        if (!is_last)
        {
            actual_ok.status_flags |= detail::SERVER_MORE_RESULTS_EXISTS;
        }
        if (!spec.empty())
        {
            meta(spec.types);
        }
        if (!spec.r.empty())
        {
            rows(spec.r);
        }
        return ok(actual_ok);
    }

    exec_builder& last_resultset(const resultset_spec& spec) { return resultset(spec, true); }

    detail::execution_state_impl build()
    {
        res_.rows().offsets_to_string_views();
        return std::move(res_);
    }

    execution_state build_state()
    {
        execution_state res;
        detail::execution_state_access::get_impl(res) = std::move(*this).build();
        return res;
    }
};

inline results create_results(const std::vector<resultset_spec>& spec)
{
    exec_builder builder(true);
    for (std::size_t i = 0; i < spec.size(); ++i)
    {
        builder.resultset(spec[i], i == spec.size() - 1);
    }
    results res;
    detail::results_access::get_impl(res) = std::move(builder).build();
    return res;
}

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
