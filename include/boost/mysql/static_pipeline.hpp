//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_STATIC_PIPELINE_HPP
#define BOOST_MYSQL_STATIC_PIPELINE_HPP

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

#include <boost/core/ignore_unused.hpp>
#include <boost/core/span.hpp>
#include <boost/mp11/tuple.hpp>
#include <boost/system/result.hpp>
#include <boost/throw_exception.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {

// Execute
class writable_field_arg
{
    field_view impl_;
#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif
public:
    template <class WritableField>  // TODO: concept
    writable_field_arg(const WritableField& f) noexcept : impl_(detail::to_field(f))
    {
    }
};

class execute_stage
{
    // TODO: could we unify this and any_execution_request?
    enum type_t
    {
        type_query,
        type_stmt_tuple,
        type_stmt_range
    };

    union data_t
    {
        string_view query;
        struct
        {
            statement stmt;
            span<const writable_field_arg> params;
        } stmt_tuple;
        struct
        {
            statement stmt;
            span<const field_view> params;
        } stmt_range;

        data_t(string_view q) noexcept : query(q) {}
        data_t(statement s, span<const writable_field_arg> params) noexcept : stmt_tuple{s, params} {}
        data_t(statement s, span<const field_view> params) noexcept : stmt_range{s, params} {}
    };

    type_t type_;
    data_t data_;

    // TODO: move to compiled
    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const
    {
        switch (type_)
        {
        case type_query: return detail::serialize_query(buffer, data_.query);
        case type_stmt_tuple:
        {
            constexpr std::size_t stack_fields = 64u;
            auto params = data_.stmt_tuple.params;
            auto stmt = data_.stmt_tuple.stmt;
            if (params.size() <= stack_fields)
            {
                std::array<field_view, stack_fields> storage;
                for (std::size_t i = 0; i < params.size(); ++i)
                    storage[i] = detail::access::get_impl(params[i]);
                return detail::serialize_execute_statement(buffer, stmt, {storage.data(), params.size()});
            }
            else
            {
                std::vector<field_view> storage;
                storage.reserve(params.size());
                for (auto p : params)
                    storage.push_back(detail::access::get_impl(p));
                return detail::serialize_execute_statement(buffer, stmt, storage);
            }
        }
        case type_stmt_range:
            return detail::serialize_execute_statement(
                buffer,
                data_.stmt_range.stmt,
                data_.stmt_range.params
            );
        }
    }

#ifndef BOOST_MYSQL_DOXYGEN
    template <class... StageType>
    friend class static_pipeline_request;
#endif

public:
    execute_stage(string_view query) : type_(type_query), data_(query) {}
    execute_stage(statement stmt, std::initializer_list<writable_field_arg> params)
        : type_(type_stmt_tuple), data_(stmt, {params.begin(), params.size()})
    {
    }
    execute_stage(statement stmt, span<const field_view> params) : type_(type_stmt_range), data_(stmt, params)
    {
    }
    using response_type = system::result<results, errcode_and_diagnostics>;
};

// Prepare statement
class prepare_statement_stage
{
    string_view stmt_sql_;

    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const
    {
        return detail::serialize_prepare_statement(buffer, stmt_sql_);
    }

#ifndef BOOST_MYSQL_DOXYGEN
    template <class... StageType>
    friend class static_pipeline_request;
#endif

public:
    prepare_statement_stage(string_view stmt_sql) : stmt_sql_(stmt_sql) {}

    using response_type = system::result<statement, errcode_and_diagnostics>;
};

// Close statement
class close_statement_stage
{
    std::uint32_t stmt_id_;

    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const
    {
        return detail::serialize_close_statement(buffer, stmt_id_);
    }

#ifndef BOOST_MYSQL_DOXYGEN
    template <class... StageType>
    friend class static_pipeline_request;
#endif

public:
    close_statement_stage(statement stmt) : stmt_id_(stmt.id()) {}
    using response_type = errcode_and_diagnostics;
};

// Reset connection
class reset_connection_stage
{
    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const
    {
        return detail::serialize_reset_connection(buffer);
    }

#ifndef BOOST_MYSQL_DOXYGEN
    template <class... StageType>
    friend class static_pipeline_request;
#endif

public:
    reset_connection_stage() = default;
    using response_type = errcode_and_diagnostics;
};

// Set character set
class set_character_set_stage
{
    character_set charset_;

    detail::pipeline_request_stage create(std::vector<std::uint8_t>& buffer) const
    {
        return detail::serialize_set_character_set(buffer, charset_);
    }

#ifndef BOOST_MYSQL_DOXYGEN
    template <class... StageType>
    friend class static_pipeline_request;
#endif

public:
    set_character_set_stage(character_set charset) : charset_(charset) {}
    using response_type = errcode_and_diagnostics;
};

template <class... StageType>
class static_pipeline_request
{
    static_assert(sizeof...(StageType) > 0u, "A pipeline should have one stage, at least");

    using stage_array_t = std::array<detail::pipeline_request_stage, sizeof...(StageType)>;

    static stage_array_t create_stage_array(std::vector<std::uint8_t>& buff, const StageType&... args)
    {
        return {{args.create(buff)...}};
    }

    struct impl_t
    {
        std::vector<std::uint8_t> buffer_;
        stage_array_t stages_;

        impl_t(const StageType&... args) : stages_(create_stage_array(buffer_, args...)) {}
    } impl_;

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

public:
    static_pipeline_request(const StageType&... args) : impl_(args...) {}

    void reset(const StageType&... args)
    {
        impl_.buffer_.clear();
        impl_.stages_ = create_stage_array(impl_.buffer_, args...);
    }

    using response_type = std::tuple<typename StageType::response_type...>;
};

template <class... Args>
static_pipeline_request<Args...> make_pipeline_request(const Args&... args)
{
    return {args...};
}

template <class... Args>
static_pipeline_request(const Args&... args) -> static_pipeline_request<Args...>;

}  // namespace mysql
}  // namespace boost

// TODO: move to separate header?
// Implementations
namespace boost {
namespace mysql {
namespace detail {

// Index a tuple at runtime
template <std::size_t I, class R>
struct tuple_index_visitor
{
    template <class... T, class Fn, class... Args>
    static R invoke(std::tuple<T...>& t, std::size_t i, Fn f, Args&&... args)
    {
        return i == I ? f(std::get<I>(t), std::forward<Args>(args)...)
                      : tuple_index_visitor<I - 1, R>::invoke(t, i, f, std::forward<Args>(args)...);
    }
};

template <class R>
struct tuple_index_visitor<0u, R>
{
    template <class... T, class Fn, class... Args>
    static R invoke(std::tuple<T...>& t, std::size_t i, Fn f, Args&&... args)
    {
        BOOST_ASSERT(i == 0u);
        ignore_unused(i);
        return f(std::get<0u>(t), std::forward<Args>(args)...);
    }
};

template <class... T, class Fn, class... Args>
auto tuple_index(std::tuple<T...>& t, std::size_t i, Fn f, Args&&... args)
    -> decltype(f(std::get<0u>(t), std::forward<Args>(args)...))
{
    static_assert(sizeof...(T) > 0u, "Empty tuples not allowed");
    BOOST_ASSERT(i < sizeof...(T));
    using result_type = decltype(f(std::get<0u>(t), std::forward<Args>(args)...));
    return tuple_index_visitor<sizeof...(T) - 1, result_type>::invoke(t, i, f, std::forward<Args>(args)...);
}

// Concrete visitors
struct stage_reset_visitor
{
    void operator()(execute_stage::response_type& value) const { value.emplace(); }
    void operator()(prepare_statement_stage::response_type& value) const { value.emplace(); }

    void operator()(errcode_and_diagnostics& value) const
    {
        value.code.clear();
        value.diag.clear();
    }
};

struct stage_get_processor_visitor
{
    execution_processor* operator()(execute_stage::response_type& value) const
    {
        return &access::get_impl(*value);
    }

    template <class T>
    execution_processor* operator()(T&) const
    {
        BOOST_ASSERT(false);
        return nullptr;
    }
};

struct stage_set_result_visitor
{
    void operator()(prepare_statement_stage::response_type& r, statement s) const { r = s; }

    template <class T>
    void operator()(T&, statement) const
    {
        BOOST_ASSERT(false);
    }
};

struct stage_set_error_visitor
{
    template <class T>
    void operator()(T& r, error_code ec, diagnostics&& diag) const
    {
        r = errcode_and_diagnostics{ec, std::move(diag)};
    }
};

template <class... StageResponseType>
struct pipeline_response_traits<std::tuple<StageResponseType...>>
{
    using response_type = std::tuple<StageResponseType...>;

    static void setup(response_type& self, span<const pipeline_request_stage> request)
    {
        BOOST_ASSERT(request.size() == sizeof...(StageResponseType));
        ignore_unused(request);
        mp11::tuple_for_each(self, stage_reset_visitor{});
    }

    static execution_processor& get_processor(response_type& self, std::size_t idx)
    {
        return *tuple_index(self, idx, stage_get_processor_visitor{});
    }

    static void set_result(response_type& self, std::size_t idx, statement stmt)
    {
        tuple_index(self, idx, stage_set_result_visitor{}, stmt);
    }

    static void set_error(response_type& self, std::size_t idx, error_code ec, diagnostics&& diag)
    {
        tuple_index(self, idx, stage_set_error_visitor{}, ec, std::move(diag));
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
