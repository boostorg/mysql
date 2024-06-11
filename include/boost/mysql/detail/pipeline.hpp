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
#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/resultset_encoding.hpp>

#include <boost/core/span.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

class execution_processor;

enum class pipeline_stage_kind
{
    execute,
    prepare_statement,
    close_statement,
    reset_connection,
    set_character_set,
    ping,
};

struct pipeline_request_stage
{
    pipeline_stage_kind kind;
    std::uint8_t seqnum;
    union stage_specific_t
    {
        std::nullptr_t nothing;
        resultset_encoding enc;
        character_set charset;

        stage_specific_t() noexcept : nothing() {}
        stage_specific_t(resultset_encoding v) noexcept : enc(v) {}
        stage_specific_t(character_set v) noexcept : charset(v) {}
    } stage_specific;
};

struct pipeline_request_view
{
    span<const std::uint8_t> buffer;
    span<const pipeline_request_stage> stages;
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
            span<const pipeline_request_stage> request_stages;

            static_assert(std::is_standard_layout<decltype(request_stages)>::value, "Internal error");
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
            const error_code* ec;
            diagnostics* diag;
        } set_error;

        fn_args(span<const pipeline_request_stage> v) noexcept : setup{fn_type_setup, v} {}
        fn_args(std::size_t index) noexcept : get_processor{fn_type_get_processor, index} {}
        fn_args(std::size_t index, statement stmt) noexcept : set_result{fn_type_set_result, index, stmt} {}
        fn_args(std::size_t index, const error_code* ec, diagnostics* diag) noexcept
            : set_error{fn_type_set_error, index, ec, diag}
        {
        }
    };

    static execution_processor* null_invoke(void*, fn_args) { return nullptr; }

    template <class T>
    static execution_processor* do_invoke(void* obj, fn_args args)
    {
        using traits_t = pipeline_response_traits<T>;

        auto& self = *static_cast<T*>(obj);
        switch (args.setup.type)
        {
        case fn_type_setup: traits_t::setup(self, args.setup.request_stages); return nullptr;
        case fn_type_get_processor: return &traits_t::get_processor(self, args.get_processor.index);
        case fn_type_set_result:
            traits_t::set_result(self, args.set_result.index, args.set_result.stmt);
            return nullptr;
        case fn_type_set_error:
            traits_t::set_error(
                self,
                args.set_error.index,
                *args.set_error.ec,
                std::move(*args.set_error.diag)
            );
            return nullptr;
        default: BOOST_ASSERT(false); return nullptr;
        }
    }

    void* obj_;
    execution_processor* (*fn_)(void*, fn_args);

public:
    pipeline_response_ref() : obj_(nullptr), fn_(&null_invoke) {}

    template <class T, class = typename std::enable_if<!std::is_same<T, pipeline_response_ref>::value>::type>
    pipeline_response_ref(T& obj) : obj_(&obj), fn_(&do_invoke<T>)
    {
    }

    void setup(span<const pipeline_request_stage> request_stages) { fn_(obj_, {request_stages}); }
    execution_processor& get_processor(std::size_t stage_idx) { return *fn_(obj_, {stage_idx}); }
    void set_result(std::size_t stage_idx, statement result) { fn_(obj_, {stage_idx, result}); }
    void set_error(std::size_t stage_idx, error_code ec, diagnostics&& diag)
    {
        fn_(obj_, {stage_idx, &ec, &diag});
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
