//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ROWS_IPP
#define BOOST_MYSQL_IMPL_ROWS_IPP

#pragma once

#include <boost/mysql/rows.hpp>
#include <stdexcept>


inline void boost::mysql::rows::rebase_strings(const char* old_buffer_base)
{
    const char* new_buffer_base = string_buffer_.data();
    auto diff = new_buffer_base - old_buffer_base;
    if (diff)
    {
        for (auto& f: fields_)
        {
            const boost::string_view* str = f.if_string();
            if (str)
            {
                f = field_view(boost::string_view(
                    str->data() + diff,
                    str->size()
                ));
            }
        }
    }
}

inline boost::mysql::row_view boost::mysql::rows::at(
    std::size_t i
) const
{
    if (i >= size())
    {
        throw std::out_of_range("rows::at");
    }
    return (*this)[i];
}

inline void boost::mysql::rows::copy_strings(std::size_t field_offset)
{
    // Calculate the extra size required for the new strings
    std::size_t size = 0;
    for (const auto* f = fields_.data() + field_offset; f != fields_.data() + fields_.size(); ++f)
    {
        const boost::string_view* str = f->if_string();
        if (str)
        {
            size += str->size();
        }
    }

    // Make space and rebase the old strings
    const char* old_buffer_base = string_buffer_.data();
    std::size_t old_buffer_size = string_buffer_.size();
    string_buffer_.resize(size);
    rebase_strings(old_buffer_base);
    
    // Copy the strings
    std::size_t offset = old_buffer_size;
    for (auto& f: fields_)
    {
        const boost::string_view* str = f.if_string();
        if (str)
        {
            std::memcpy(string_buffer_.data() + offset, str->data(), str->size());
            f = field_view(boost::string_view(string_buffer_.data() + offset, str->size()));
            offset += str->size();
        }
    }
}

#endif
