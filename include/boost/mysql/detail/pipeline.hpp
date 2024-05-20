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
    void* obj_;
    void (*clear_fn_)(void*);
    execution_processor* (*setup_step_fn_)(void*, pipeline_step_kind, std::size_t);
    void (*set_step_result_fn_)(void*, err_block&&, statement, std::size_t);

    template <class T>
    static void do_clear(void* obj)
    {
        pipeline_response_traits<T>::clear(*static_cast<T*>(obj));
    }

    template <class T>
    static execution_processor* do_setup_step(void* obj, pipeline_step_kind kind, std::size_t idx)
    {
        return pipeline_response_traits<T>::setup_step(*static_cast<T*>(obj), kind, idx);
    }

    template <class T>
    static void do_set_step_result(void* obj, err_block&& err, statement stmt, std::size_t idx)
    {
        pipeline_response_traits<T>::set_step_result(*static_cast<T*>(obj), std::move(err), stmt, idx);
    }

public:
    template <class T, class = typename std::enable_if<!std::is_same<T, pipeline_response_ref>::value>::type>
    pipeline_response_ref(T& obj)
        : obj_(&obj),
          clear_fn_(&do_clear<T>),
          setup_step_fn_(&do_setup_step<T>),
          set_step_result_fn_(&do_set_step_result<T>)
    {
    }

    void clear() { clear_fn_(obj_); }
    execution_processor* setup_step(pipeline_step_kind kind, std::size_t idx)
    {
        return setup_step_fn_(obj_, kind, idx);
    }
    void set_step_result(err_block&& err, statement stmt, std::size_t idx)
    {
        set_step_result_fn_(obj_, std::move(err), stmt, idx);
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
