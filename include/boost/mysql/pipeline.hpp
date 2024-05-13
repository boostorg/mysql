//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_PIPELINE_HPP
#define BOOST_MYSQL_PIPELINE_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/pipeline_step_kind.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_concepts.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/mysql/impl/statement.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

BOOST_MYSQL_DECL void serialize_query(std::vector<std::uint8_t>& buffer, string_view query);
BOOST_MYSQL_DECL void serialize_execute_statement(
    std::vector<std::uint8_t>& buffer,
    statement stmt,
    span<const field_view> params
);
BOOST_MYSQL_DECL void serialize_prepare_statement(std::vector<std::uint8_t>& buffer, string_view stmt_sql);
BOOST_MYSQL_DECL void serialize_close_statement(std::vector<std::uint8_t>& buffer, statement stmt);
BOOST_MYSQL_DECL void serialize_set_character_set(std::vector<std::uint8_t>& buffer, character_set charset);
BOOST_MYSQL_DECL void serialize_reset_connection(std::vector<std::uint8_t>& buffer);
BOOST_MYSQL_DECL void serialize_ping(std::vector<std::uint8_t>& buffer);

}  // namespace detail

class pipeline_step
{
    struct visitor
    {
        detail::pipeline_step_descriptor::step_specific_t operator()(variant2::monostate) const { return {}; }
        detail::pipeline_step_descriptor::step_specific_t operator()(results& r) const
        {
            // TODO: this is a little bit weird
            auto& proc = detail::access::get_impl(r);
            return detail::pipeline_step_descriptor::execute_t{proc.encoding(), proc.meta_mode(), &proc};
        }
        detail::pipeline_step_descriptor::step_specific_t operator()(statement& s) const
        {
            return detail::pipeline_step_descriptor::prepare_statement_t{&s};
        }
        detail::pipeline_step_descriptor::step_specific_t operator()(character_set c) const
        {
            return detail::pipeline_step_descriptor::set_character_set_t{c};
        }
    };

    struct impl_t
    {
        pipeline_step_kind kind;
        detail::pipeline_step_error err;
        std::uint8_t seqnum;
        variant2::variant<variant2::monostate, results, statement, character_set>
            result;  // TODO: this sizeof is high

        detail::pipeline_step_descriptor to_descriptor()
        {
            return {kind, &err, seqnum, variant2::visit(visitor{}, result)};
        }
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif
public:
    // TODO: these should be private
    pipeline_step(pipeline_step_kind k, std::uint8_t seqnum) noexcept : impl_{k, {}, seqnum, {}}
    {
        if (k == pipeline_step_kind::prepare_statement)
            impl_.result.emplace<statement>();
    }
    pipeline_step(detail::resultset_encoding enc, metadata_mode meta, std::uint8_t seqnum) noexcept
        : impl_{pipeline_step_kind::execute, {}, seqnum, results{}}  // TODO: construct that in place
    {
        detail::access::get_impl(variant2::unsafe_get<1>(impl_.result)).reset(enc, meta);
    }
    pipeline_step(character_set charset, std::uint8_t seqnum) noexcept
        : impl_{pipeline_step_kind::set_character_set, {}, seqnum, charset}
    {
    }

    pipeline_step_kind kind() const noexcept { return impl_.kind; }
    error_code error() const noexcept { return impl_.err.ec; }
    const diagnostics& diag() const noexcept { return impl_.err.diag; }
    const results& execute_result() const
    {
        BOOST_ASSERT(impl_.kind == pipeline_step_kind::execute);
        return variant2::get<results>(impl_.result);
    }
    statement prepare_statement_result() const
    {
        BOOST_ASSERT(impl_.kind == pipeline_step_kind::execute);
        return variant2::get<statement>(impl_.result);
    }
};

class pipeline
{
    struct impl_t
    {
        std::vector<std::uint8_t> buffer_;
        std::vector<pipeline_step> steps_;

        void clear()
        {
            buffer_.clear();
            steps_.clear();
        }

        detail::pipeline_step_descriptor step_descriptor_at(std::size_t idx)
        {
            return idx >= steps_.size() ? detail::pipeline_step_descriptor{}
                                        : detail::access::get_impl(steps_[idx]).to_descriptor();
        }

    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

    BOOST_MYSQL_DECL void add_query_step(string_view sql, metadata_mode meta);

    template <class... Params>
    void add_statement_execute_step(statement stmt, metadata_mode meta, const Params&... params)
    {
        std::array<field_view, sizeof...(Params)> params_array{{detail::to_field(params)...}};
        add_statement_execute_step_range(stmt, params_array, meta);
    }

    BOOST_MYSQL_DECL void add_statement_execute_step_range(
        statement stmt,
        span<const field_view> params,
        metadata_mode meta
    );

public:
    pipeline() = default;
    void clear() noexcept { impl_.clear(); }

    // Adding steps
    // TODO: would execution requests be less confusing?
    void add_execute(string_view query, metadata_mode meta = metadata_mode::minimal)
    {
        add_query_step(query, meta);
    }

    template <class... Params>
    void add_execute(statement stmt, const Params&... params)
    {
        add_statement_execute_step(stmt, metadata_mode::minimal, params...);
    }

    template <class... Params>
    void add_execute(statement stmt, metadata_mode meta, const Params&... params)
    {
        add_statement_execute_step(stmt, meta, params...);
    }

    void add_execute(
        statement stmt,
        span<const field_view> params,
        metadata_mode meta = metadata_mode::minimal
    )
    {
        add_statement_execute_step_range(stmt, params, meta);
    }

    BOOST_MYSQL_DECL
    void add_prepare_statement(string_view statement_sql);

    BOOST_MYSQL_DECL
    void add_close_statement(statement stmt);

    BOOST_MYSQL_DECL
    void add_set_character_set(character_set charset);

    BOOST_MYSQL_DECL
    void add_reset_connection();

    BOOST_MYSQL_DECL
    void add_ping();

    // Retrieving results
    span<const pipeline_step> steps() const noexcept { return impl_.steps_; }
};

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/pipeline.ipp>
#endif

#endif
