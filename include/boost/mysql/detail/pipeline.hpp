//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PIPELINE_HPP
#define BOOST_MYSQL_DETAIL_PIPELINE_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/pipeline_step_kind.hpp>
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/core/span.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

// TODO: do we want to rename this file?

namespace boost {
namespace mysql {
namespace detail {

struct err_block
{
    error_code ec;
    diagnostics diag;
};

struct pipeline_request_step
{
    pipeline_step_kind kind;
    std::uint8_t seqnum;
    union step_specific_t
    {
        std::nullptr_t nothing;
        resultset_encoding enc;
        character_set charset;

        step_specific_t() noexcept : nothing() {}
        step_specific_t(resultset_encoding v) noexcept : enc(v) {}
        step_specific_t(character_set v) noexcept : charset(v) {}
    } step_specific;
};

template <class T>
struct pipeline_response_traits;

class pipeline_response_ref
{
    // We multiplex calls to store a single function pointer, instead of four
    enum fn_type
    {
        fn_type_setup,
        fn_type_get_processor,
        fn_type_set_result,
        fn_type_set_error
    };

    union fn_args
    {
        struct setup_t
        {
            fn_type type;
            span<const pipeline_request_step> request_steps;

            static_assert(std::is_standard_layout<decltype(request_steps)>::value, "Internal error");
        } setup;
        struct get_processor_t
        {
            fn_type type;
            std::size_t index;
        } get_processor;
        struct set_result_t
        {
            fn_type type;
            std::size_t index;
            statement stmt;
        } set_result;
        struct set_error_t
        {
            fn_type type;
            std::size_t index;
            err_block* err;
        } set_error;

        fn_args(span<const pipeline_request_step> v) noexcept : setup{fn_type_setup, v} {}
        fn_args(std::size_t index) noexcept : get_processor{fn_type_get_processor, index} {}
        fn_args(std::size_t index, statement stmt) noexcept : set_result{fn_type_set_result, index, stmt} {}
        fn_args(std::size_t index, err_block* err) noexcept : set_error{fn_type_set_error, index, err} {}
    };

    void* obj_;
    execution_processor* (*fn_)(void*, fn_args);

    template <class T>
    static execution_processor* do_invoke(void* obj, fn_args args)
    {
        using traits_t = pipeline_response_traits<T>;

        auto& self = *static_cast<T*>(obj);
        switch (args.setup.type)
        {
        case fn_type_setup: traits_t::setup(self, args.setup.request_steps); return nullptr;
        case fn_type_get_processor: return &traits_t::get_processor(self, args.get_processor.index);
        case fn_type_set_result:
            traits_t::set_result(self, args.set_result.index, args.set_result.stmt);
            return nullptr;
        case fn_type_set_error:
            traits_t::set_error(self, args.set_error.index, std::move(*args.set_error.err));
            return nullptr;
        }
    }

public:
    template <class T, class = typename std::enable_if<!std::is_same<T, pipeline_response_ref>::value>::type>
    pipeline_response_ref(T& obj) : obj_(&obj), fn_(&do_invoke<T>)
    {
    }

    void setup(span<const pipeline_request_step> request_steps) { fn_(obj_, {request_steps}); }
    execution_processor& get_processor(std::size_t step_idx) { return *fn_(obj_, {step_idx}); }
    void set_result(std::size_t step_idx, statement result) { fn_(obj_, {step_idx, result}); }
    void set_error(std::size_t step_idx, err_block&& result) { fn_(obj_, {step_idx, &result}); }
};

BOOST_MYSQL_DECL std::uint8_t serialize_query(std::vector<std::uint8_t>& buffer, string_view query);
BOOST_MYSQL_DECL std::uint8_t serialize_execute_statement(
    std::vector<std::uint8_t>& buffer,
    statement stmt,
    span<const field_view> params
);
BOOST_MYSQL_DECL std::uint8_t serialize_prepare_statement(
    std::vector<std::uint8_t>& buffer,
    string_view stmt_sql
);
BOOST_MYSQL_DECL std::uint8_t serialize_close_statement(std::vector<std::uint8_t>& buffer, statement stmt);
BOOST_MYSQL_DECL std::uint8_t serialize_set_character_set(
    std::vector<std::uint8_t>& buffer,
    character_set charset
);
BOOST_MYSQL_DECL std::uint8_t serialize_reset_connection(std::vector<std::uint8_t>& buffer);
BOOST_MYSQL_DECL std::uint8_t serialize_ping(std::vector<std::uint8_t>& buffer);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/pipeline.ipp>
#endif

#endif
