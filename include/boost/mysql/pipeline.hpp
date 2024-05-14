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

class pipeline_step
{
    struct visitor
    {
        detail::pipeline_step_descriptor::step_specific_t operator()(variant2::monostate) const { return {}; }
        detail::pipeline_step_descriptor::step_specific_t operator()(results& r) const
        {
            return detail::pipeline_step_descriptor::execute_t{&detail::access::get_impl(r)};
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
    pipeline_step(detail::resultset_encoding enc, std::uint8_t seqnum) noexcept
        : impl_{pipeline_step_kind::execute, {}, seqnum, results{}}  // TODO: construct that in place
    {
        detail::access::get_impl(variant2::unsafe_get<1>(impl_.result)).reset(enc, metadata_mode::minimal);
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

public:
    pipeline() = default;
    void clear() noexcept { impl_.clear(); }

    // Adding steps
    // TODO: would execution requests be less confusing?
    void add_execute(string_view query)
    {
        impl_.steps_.emplace_back(
            detail::resultset_encoding::text,
            detail::serialize_query(impl_.buffer_, query)
        );
    }

    template <class... Params>
    void add_execute(statement stmt, const Params&... params)
    {
        std::array<field_view, sizeof...(Params)> params_array{{detail::to_field(params)...}};
        add_execute(stmt, params_array);
    }

    void add_execute(statement stmt, span<const field_view> params)
    {
        impl_.steps_.emplace_back(
            detail::resultset_encoding::binary,
            detail::serialize_execute_statement(impl_.buffer_, stmt, params)
        );
    }

    void add_prepare_statement(string_view statement_sql)
    {
        impl_.steps_.emplace_back(
            pipeline_step_kind::prepare_statement,
            detail::serialize_prepare_statement(impl_.buffer_, statement_sql)
        );
    }

    void add_close_statement(statement stmt)
    {
        impl_.steps_.emplace_back(
            pipeline_step_kind::close_statement,
            detail::serialize_close_statement(impl_.buffer_, stmt)
        );
    }

    void add_set_character_set(character_set charset)
    {
        impl_.steps_.emplace_back(charset, detail::serialize_set_character_set(impl_.buffer_, charset));
    }

    void add_reset_connection()
    {
        impl_.steps_.emplace_back(
            pipeline_step_kind::reset_connection,
            detail::serialize_reset_connection(impl_.buffer_)
        );
    }

    void add_ping()
    {
        impl_.steps_.emplace_back(pipeline_step_kind::ping, detail::serialize_ping(impl_.buffer_));
    }

    // Retrieving results
    span<const pipeline_step> steps() const noexcept { return impl_.steps_; }
};

}  // namespace mysql
}  // namespace boost

#endif
