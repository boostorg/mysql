//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ROW_HPP
#define BOOST_MYSQL_IMPL_ROW_HPP

#pragma once

#include <boost/mysql/row.hpp>


inline void boost::mysql::row::rebase_strings(
    const char* old_buffer_base
) noexcept
{
    auto diff = string_buffer_.data() - old_buffer_base;
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

inline void boost::mysql::row::copy_strings()
{
    // Calculate size
    std::size_t size = 0;
    for (const auto& f: fields_)
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
        if (str)
        {
            std::memcpy(string_buffer_.data() + offset, str->data(), str->size());
            f = field_view(boost::string_view(string_buffer_.data() + offset, str->size()));
            offset += str->size();
        }
    }
}

inline boost::mysql::row::iterator boost::mysql::row::to_row_it(
    std::vector<field_view>::iterator it
) noexcept
{
    return it == fields_.end() ? nullptr : fields_.data() + (it - fields_.begin());
}

inline std::vector<boost::mysql::field_view>::iterator boost::mysql::row::to_vector_it(
    iterator it
) noexcept
{
    return it == nullptr ? fields_.end() : fields_.begin() + (it - fields_.data());
}

boost::mysql::row::row(
    const row& other
) :
    fields_(other.fields_),
    string_buffer_(other.string_buffer_)
{
    rebase_strings(other.string_buffer_.data());
}


const boost::mysql::row& boost::mysql::row::operator=(
    const row& rhs
)
{
    fields_ = rhs.fields_;
    string_buffer_ = rhs.string_buffer_;
    rebase_strings(rhs.string_buffer_.data());
    return *this;
}

inline boost::string_view boost::mysql::row::copy_string(
    boost::string_view v
)
{
    const char* old_buffer_base = string_buffer_.data();
    std::size_t old_buffer_size = string_buffer_.size();
    string_buffer_.insert(string_buffer_.end(), v.data(), v.data() + v.size());
    if (string_buffer_.data() != old_buffer_base)
        rebase_strings(old_buffer_base);
    return boost::string_view(string_buffer_.data() + old_buffer_size, v.size());
}

inline boost::mysql::row::iterator boost::mysql::row::insert(
    iterator before,
    field_view v
)
{
    const auto* str = v.if_string();
    if (str)
    {
        v = field_view(copy_string(*str));
    }
    auto res = fields_.insert(to_vector_it(before), v);
    return to_row_it(res);
}

// inline boost::mysql::row::iterator boost::mysql::row::insert(
//     iterator before,
//     std::initializer_list<field_view> v
// )
// {
//     // Calculate the extra size required for the strings
//     std::size_t new_string_size = 0;
//     for (const auto& f: v)
//     {
//         const auto* str = f.if_string();
//         if (str)
//         {
//             new_string_size += str->size();
//         }
//     }


// }


#endif
