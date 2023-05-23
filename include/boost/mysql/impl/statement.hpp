//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_STATEMENT_HPP
#define BOOST_MYSQL_IMPL_STATEMENT_HPP

#pragma once

#include <boost/mysql/statement.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/protocol/prepared_statement_messages.hpp>

#include <boost/assert.hpp>

template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple>
class boost::mysql::bound_statement_tuple
{
    friend class statement;
    friend struct detail::statement_access;

    struct impl
    {
        statement stmt;
        WritableFieldTuple params;
    } impl_;

    template <typename TupleType>
    bound_statement_tuple(const statement& stmt, TupleType&& t) : impl_{stmt, std::forward<TupleType>(t)}
    {
    }
};

template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
class boost::mysql::bound_statement_iterator_range
{
    friend class statement;
    friend struct detail::statement_access;

    struct impl
    {
        statement stmt;
        FieldViewFwdIterator first;
        FieldViewFwdIterator last;
    } impl_;

    bound_statement_iterator_range(
        const statement& stmt,
        FieldViewFwdIterator first,
        FieldViewFwdIterator last
    )
        : impl_{stmt, first, last}
    {
    }
};

template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple, typename EnableIf>
boost::mysql::bound_statement_tuple<typename std::decay<WritableFieldTuple>::type> boost::mysql::statement::
    bind(WritableFieldTuple&& args) const

{
    BOOST_ASSERT(valid());
    return bound_statement_tuple<typename std::decay<WritableFieldTuple>::type>(
        *this,
        std::forward<WritableFieldTuple>(args)
    );
}

template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator, typename EnableIf>
boost::mysql::bound_statement_iterator_range<FieldViewFwdIterator> boost::mysql::statement::bind(
    FieldViewFwdIterator first,
    FieldViewFwdIterator last
) const
{
    BOOST_ASSERT(valid());
    return bound_statement_iterator_range<FieldViewFwdIterator>(*this, first, last);
}

struct boost::mysql::detail::statement_access
{
    static void reset(statement& stmt, const detail::com_stmt_prepare_ok_packet& msg) noexcept
    {
        stmt.valid_ = true;
        stmt.id_ = msg.statement_id;
        stmt.num_params_ = msg.num_params;
    }

    template <BOOST_MYSQL_WRITABLE_FIELD_TUPLE WritableFieldTuple>
    static const typename bound_statement_tuple<WritableFieldTuple>::impl get_impl_tuple(
        const bound_statement_tuple<WritableFieldTuple>& obj
    )
    {
        return obj.impl_;
    }

    template <BOOST_MYSQL_FIELD_VIEW_FORWARD_ITERATOR FieldViewFwdIterator>
    static const typename bound_statement_iterator_range<FieldViewFwdIterator>::impl get_impl_range(
        const bound_statement_iterator_range<FieldViewFwdIterator>& obj
    )
    {
        return obj.impl_;
    }
};

#endif
