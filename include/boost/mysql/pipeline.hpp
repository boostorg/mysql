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
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_concepts.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/mysql/impl/statement.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace boost {
namespace mysql {

enum class pipeline_step_kind
{
    execute,
    prepare_statement,
    close_statement,
    reset_connection,
    set_character_set,
    ping,
};

namespace detail {

struct pipeline_step_impl
{
    pipeline_step_kind kind;
    error_code ec;
    diagnostics diag;
    // TODO: having character_set here works but isn't nice
    variant2::variant<detail::execution_processor*, results, statement, character_set> result;

    std::uint8_t seqnum;

    void reset()
    {
        ec.clear();
        diag.clear();
    }

    void set_error(error_code e) { ec = e; }

    detail::execution_processor& get_processor()
    {
        BOOST_ASSERT(kind == pipeline_step_kind::execute);
        auto* res = variant2::get_if<results>(&result);
        if (res)
            return detail::access::get_impl(*res).get_interface();
        return *variant2::get<detail::execution_processor*>(result);
    }
};

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
    detail::pipeline_step_impl impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif
public:
    // TODO: make this private - noexceptness
    pipeline_step(detail::pipeline_step_impl&& impl) : impl_(std::move(impl)) {}

    pipeline_step_kind kind() const noexcept { return impl_.kind; }
    bool uses_external_results() const noexcept
    {
        return impl_.result.index() == 0u && variant2::unsafe_get<0>(impl_.result) != nullptr;
    }

    error_code error() const noexcept { return impl_.ec; }
    const diagnostics& diag() const noexcept { return impl_.diag; }
    const results& execute_result() const
    {
        BOOST_ASSERT(impl_.kind == pipeline_step_kind::execute);
        BOOST_ASSERT(!uses_external_results());
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

        void reset_results()
        {
            for (auto& step : steps_)
                detail::access::get_impl(step).reset();
        }

        void clear()
        {
            buffer_.clear();
            steps_.clear();
        }
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

    template <class ResultsType>
    static detail::execution_processor* to_processor(ResultsType& r)
    {
        return detail::access::get_impl(r).get_interface();
    }

    BOOST_MYSQL_DECL void add_query_step(
        string_view sql,
        detail::execution_processor* processor,
        metadata_mode meta
    );

    template <class... Params>
    void add_statement_execute_step(
        statement stmt,
        detail::execution_processor* processor,
        metadata_mode meta,
        const Params&... params
    )
    {
        std::array<field_view, sizeof...(Params)> params_array{{detail::to_field(params)...}};
        add_statement_execute_step_range(stmt, params_array, processor, meta);
    }

    BOOST_MYSQL_DECL void add_statement_execute_step_range(
        statement stmt,
        span<const field_view> params,
        detail::execution_processor* processor,
        metadata_mode meta
    );

public:
    pipeline() = default;
    void clear() noexcept { impl_.clear(); }

    // Adding steps
    // TODO: would execution requests be less confusing?
    void add_execute(string_view query, metadata_mode meta = metadata_mode::minimal)
    {
        add_query_step(query, nullptr, meta);
    }

    template <BOOST_MYSQL_RESULTS_TYPE ResultsType>
    void add_execute(string_view query, ResultsType& result, metadata_mode meta = metadata_mode::minimal)
    {
        add_query_step(query, to_processor(result), meta);
    }

    template <class... Params>
    void add_execute(statement stmt, const Params&... params)
    {
        add_statement_execute_step(stmt, nullptr, metadata_mode::minimal, params...);
    }

    template <BOOST_MYSQL_RESULTS_TYPE ResultsType, class... Params>
    void add_execute(statement stmt, ResultsType& result, const Params&... params)
    {
        add_statement_execute_step(stmt, to_processor(result), metadata_mode::minimal, params...);
    }

    template <class... Params>
    void add_execute(statement stmt, metadata_mode meta, const Params&... params)
    {
        add_statement_execute_step(stmt, nullptr, meta, params...);
    }

    template <BOOST_MYSQL_RESULTS_TYPE ResultsType, class... Params>
    void add_execute(statement stmt, ResultsType& result, metadata_mode meta, const Params&... params)
    {
        add_statement_execute_step(stmt, to_processor(result), meta, params...);
    }

    void add_execute(
        statement stmt,
        span<const field_view> params,
        metadata_mode meta = metadata_mode::minimal
    )
    {
        add_statement_execute_step_range(stmt, params, nullptr, meta);
    }

    template <BOOST_MYSQL_RESULTS_TYPE ResultsType>
    void add_execute(
        statement stmt,
        span<const field_view> params,
        ResultsType& result,
        metadata_mode meta = metadata_mode::minimal
    )
    {
        add_statement_execute_step_range(stmt, params, to_processor(result), meta);
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
