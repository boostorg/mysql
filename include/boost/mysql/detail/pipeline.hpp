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
    // get_processor, set_result and set_error are pretty similar in signature,
    // so we multiplex the call to store a single function pointer for the three of them
    enum class index_step_op
    {
        get_processor,
        set_result,
        set_error
    };

    void* obj_;
    void (*setup_fn_)(void*, span<const pipeline_request_step>);
    void (*index_step_fn_)(void*, index_step_op, std::size_t, void*);

    template <class T>
    static void do_setup(void* obj, span<const pipeline_request_step> request_steps)
    {
        pipeline_response_traits<T>::setup(*static_cast<T*>(obj), request_steps);
    }

    template <class T>
    static void do_index_step(void* obj, index_step_op op, std::size_t step_idx, void* arg)
    {
        auto& self = *static_cast<T*>(obj);
        if (op == index_step_op::get_processor)
        {
            *static_cast<execution_processor**>(arg
            ) = &pipeline_response_traits<T>::get_processor(self, step_idx);
        }
        else if (op == index_step_op::set_result)
        {
            pipeline_response_traits<T>::set_result(self, step_idx, *static_cast<const statement*>(arg));
        }
        else
        {
            BOOST_ASSERT(op == index_step_op::set_error);
            pipeline_response_traits<T>::set_error(self, step_idx, std::move(*static_cast<err_block*>(arg)));
        }
    }

public:
    template <class T, class = typename std::enable_if<!std::is_same<T, pipeline_response_ref>::value>::type>
    pipeline_response_ref(T& obj) : obj_(&obj), setup_fn_(&do_setup<T>), index_step_fn_(&do_index_step<T>)
    {
    }

    void setup(span<const pipeline_request_step> request_steps) { setup_fn_(obj_, request_steps); }

    execution_processor& get_processor(std::size_t step_idx)
    {
        execution_processor* res{};
        index_step_fn_(obj_, index_step_op::get_processor, step_idx, &res);
        return *res;
    }

    void set_result(std::size_t step_idx, statement result)
    {
        index_step_fn_(obj_, index_step_op::set_result, step_idx, &result);
    }

    void set_error(std::size_t step_idx, err_block&& result)
    {
        index_step_fn_(obj_, index_step_op::set_error, step_idx, &result);
    }
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
