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
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {

class any_stage_response
{
    variant2::variant<errcode_with_diagnostics, statement, results> impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

    bool has_error() const { return impl_.index() == 0u; }

public:
    any_stage_response() = default;

    bool has_statement() const { return impl_.index() == 1u; }
    bool has_results() const { return impl_.index() == 2u; }

    errcode_with_diagnostics error() const&
    {
        return has_error() ? variant2::unsafe_get<0>(impl_) : errcode_with_diagnostics();
    }
    errcode_with_diagnostics error() &&
    {
        return has_error() ? variant2::unsafe_get<0>(std::move(impl_)) : errcode_with_diagnostics();
    }

    statement as_statement() const { return get_statement(); }  //  TODO
    statement get_statement() const { return variant2::unsafe_get<1>(impl_); }

    const results& as_results() const& { return variant2::unsafe_get<2>(impl_); }  // TODO
    results&& as_results() && { return variant2::unsafe_get<2>(std::move(impl_)); }
    const results& get_results() const& { return variant2::unsafe_get<2>(impl_); }
    results&& get_results() && { return variant2::unsafe_get<2>(std::move(impl_)); }
};

class pipeline_request
{
    struct impl_t
    {
        std::vector<std::uint8_t> buffer_;
        std::vector<detail::pipeline_request_stage> stages_;

        void clear()
        {
            buffer_.clear();
            stages_.clear();
        }

    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    pipeline_request() = default;
    void clear() noexcept { impl_.clear(); }

    pipeline_request& add_execute(string_view query)
    {
        impl_.stages_.push_back(detail::serialize_query(impl_.buffer_, query));
        return *this;
    }

    template <BOOST_MYSQL_WRITABLE_FIELD... WritableField>
    pipeline_request& add_execute(statement stmt, const WritableField&... params)
    {
        std::array<field_view, sizeof...(WritableField)> params_array{{detail::to_field(params)...}};
        add_execute_range(stmt, params_array);
        return *this;
    }

    pipeline_request& add_execute_range(statement stmt, span<const field_view> params)
    {
        impl_.stages_.push_back(detail::serialize_execute_statement(impl_.buffer_, stmt, params));
        return *this;
    }

    pipeline_request& add_prepare_statement(string_view statement_sql)
    {
        impl_.stages_.push_back(detail::serialize_prepare_statement(impl_.buffer_, statement_sql));
        return *this;
    }

    pipeline_request& add_close_statement(statement stmt)
    {
        impl_.stages_.push_back(detail::serialize_close_statement(impl_.buffer_, stmt.id()));
        return *this;
    }

    pipeline_request& add_set_character_set(character_set charset)
    {
        impl_.stages_.push_back(detail::serialize_set_character_set(impl_.buffer_, charset));
        return *this;
    }

    pipeline_request& add_reset_connection()
    {
        impl_.stages_.push_back(detail::serialize_reset_connection(impl_.buffer_));
        return *this;
    }

    using response_type = std::vector<any_stage_response>;
};

}  // namespace mysql
}  // namespace boost

// TODO: can we move this to a separate header?
// Implementations
namespace boost {
namespace mysql {
namespace detail {

template <>
struct pipeline_response_traits<std::vector<any_stage_response>>
{
    using response_type = std::vector<any_stage_response>;

    static void setup(response_type& self, span<const pipeline_request_stage> request)
    {
        // Create as many response items as request stages
        self.resize(request.size());

        // Execution stage needs to be initialized to results objects.
        // Otherwise, clear any previous content
        for (std::size_t i = 0u; i < request.size(); ++i)
        {
            auto kind = request[i].kind;
            if (kind == pipeline_stage_kind::execute)
                access::get_impl(self[i]).emplace<results>();
            else
                access::get_impl(self[i]).emplace<errcode_with_diagnostics>();
        }
    }

    static execution_processor& get_processor(response_type& self, std::size_t idx)
    {
        BOOST_ASSERT(idx < self.size());
        auto& response_variant = access::get_impl(self[idx]);
        return access::get_impl(variant2::unsafe_get<2>(response_variant));
    }

    static void set_result(response_type& self, std::size_t idx, statement stmt)
    {
        BOOST_ASSERT(idx < self.size());
        access::get_impl(self[idx]) = stmt;
    }

    static void set_error(response_type& self, std::size_t idx, error_code ec, diagnostics&& diag)
    {
        BOOST_ASSERT(idx < self.size());
        access::get_impl(self[idx]).emplace<0>(ec, std::move(diag));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
