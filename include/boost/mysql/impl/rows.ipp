//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ROWS_IPP
#define BOOST_MYSQL_IMPL_ROWS_IPP

#include "boost/mysql/rows_view.hpp"
#include <cstddef>
#pragma once

#include <boost/mysql/rows.hpp>
#include <stdexcept>

boost::mysql::rows::rows(
    const rows_view& view
) :
    fields_(view.begin(), view.end())
{
    copy_strings();
}


boost::mysql::rows::rows(
    const rows& rhs
) :
    fields_(rhs.fields_),
    string_buffer_(rhs.string_buffer_)
{
    rebase_strings(rhs.string_buffer_.data());
}


boost::mysql::rows& boost::mysql::rows::operator=(
    const rows& rhs
)
{
    fields_ = rhs.fields_;
    string_buffer_ = rhs.string_buffer_;
    rebase_strings(rhs.string_buffer_.data());
    return *this;
}

boost::mysql::rows& boost::mysql::rows::operator=(
    const rows_view& rhs
)
{
    fields_.assign(rhs.begin(), rhs.end());
    copy_strings();
    return *this;
}

boost::mysql::row_view boost::mysql::rows::operator[](
    std::size_t i
) const noexcept
{
    return row_view(fields_.data() + num_columns_ * i, num_columns_);
}

boost::mysql::row_view boost::mysql::rows::at(
    std::size_t i
) const
{
    if (i >= size())
    {
        throw std::out_of_range("rows::at");
    }
    return (*this)[i];
}

inline void boost::mysql::rows::rebase_strings(const char* old_buffer_base)
{
    const char* new_buffer_base = string_buffer_.data();
    auto diff = new_buffer_base - old_buffer_base;
    if (diff)
    {
        for (auto& f: fields_)
        {
            const boost::string_view* str = f.if_string();
            if (str && !str->empty())
            {
                f = field_view(boost::string_view(
                    str->data() + diff,
                    str->size()
                ));
            }
        }
    }
}

inline void boost::mysql::rows::copy_strings()
{
    // Calculate the required size for the new strings
    std::size_t size = 0;
    for (const auto& f : fields_)
    {
        const boost::string_view* str = f.if_string();
        if (str)
        {
            size += str->size();
        }
    }

    // Make space
    string_buffer_.resize(size);
    
    // Copy the strings
    std::size_t offset = 0;
    for (auto& f: fields_)
    {
        const boost::string_view* str = f.if_string();
        if (str && !str->empty())
        {
            std::memcpy(string_buffer_.data() + offset, str->data(), str->size());
            f = field_view(boost::string_view(string_buffer_.data() + offset, str->size()));
            offset += str->size();
        }
    }
}

#endif
