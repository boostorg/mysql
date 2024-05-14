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

// TODO: do we want to rename this file?

namespace boost {
namespace mysql {
namespace detail {

struct pipeline_step_error
{
    error_code ec;
    diagnostics diag;

    void clear()
    {
        ec.clear();
        diag.clear();
    }
};

struct pipeline_step_descriptor
{
    struct execute_t
    {
        execution_processor* processor;
    };
    struct prepare_statement_t
    {
        statement* stmt;
    };
    struct set_character_set_t
    {
        character_set charset;
    };

    pipeline_step_kind kind;
    pipeline_step_error* err;
    std::uint8_t seqnum;
    union step_specific_t
    {
        std::nullptr_t nothing;
        execute_t execute;
        prepare_statement_t prepare_statement;
        set_character_set_t set_character_set;

        step_specific_t() noexcept : nothing() {}
        step_specific_t(execute_t v) noexcept : execute(v) {}
        step_specific_t(prepare_statement_t v) noexcept : prepare_statement(v) {}
        step_specific_t(set_character_set_t v) noexcept : set_character_set(v) {}
    } step_specific;

    bool valid() const { return err != nullptr; }
};

class pipeline_step_generator
{
    void* obj_;
    pipeline_step_descriptor (*pfn)(void* obj, std::size_t current_index);

    template <class T>
    static pipeline_step_descriptor do_step_descriptor_at(void* obj, std::size_t current_index)
    {
        return static_cast<T*>(obj)->step_descriptor_at(current_index);
    }

public:
    template <
        class T,
        class = typename std::enable_if<!std::is_same<T, pipeline_step_generator>::value>::type>
    pipeline_step_generator(T& obj) : obj_(&obj), pfn(&do_step_descriptor_at<T>)
    {
    }

    pipeline_step_descriptor step_descriptor_at(std::size_t index) { return pfn(obj_, index); }
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
