//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUXILIAR_IMPL_ROW_BASE_IPP
#define BOOST_MYSQL_DETAIL_AUXILIAR_IMPL_ROW_BASE_IPP

#pragma once

#include <boost/mysql/detail/auxiliar/row_base.hpp>

boost::mysql::detail::row_base::row_base(const field_view* fields, std::size_t size)
    : fields_(fields, fields + size)
{
    copy_strings();
}

boost::mysql::detail::row_base::row_base(const row_base& rhs)
    : fields_(rhs.fields_), string_buffer_(rhs.string_buffer_)
{
    rebase_strings(rhs.string_buffer_.data());
}

boost::mysql::detail::row_base& boost::mysql::detail::row_base::operator=(const row_base& rhs)
{
    fields_ = rhs.fields_;
    string_buffer_ = rhs.string_buffer_;
    rebase_strings(rhs.string_buffer_.data());
    return *this;
}

void boost::mysql::detail::row_base::assign(const field_view* fields, std::size_t size)
{
    fields_.assign(fields, fields + size);
    copy_strings();
}

inline void boost::mysql::detail::row_base::rebase_strings(const char* old_buffer_base)
{
    const char* new_buffer_base = string_buffer_.data();
    for (auto& f : fields_)
    {
        if (f.is_string())
        {
            auto sv = f.get_string();
            if (!sv.empty())
            {
                std::size_t offset = sv.data() - old_buffer_base;
                f = field_view(boost::string_view(new_buffer_base + offset, sv.size()));
            }
        }
    }
}

inline void boost::mysql::detail::row_base::copy_strings()
{
    // Calculate the required size for the new strings
    std::size_t size = 0;
    for (const auto& f : fields_)
    {
        if (f.is_string())
        {
            size += f.get_string().size();
        }
    }

    // Make space
    string_buffer_.resize(size);

    // Copy the strings
    std::size_t offset = 0;
    for (auto& f : fields_)
    {
        if (f.is_string())
        {
            auto str = f.get_string();
            if (!str.empty())
            {
                std::memcpy(string_buffer_.data() + offset, str.data(), str.size());
                f = field_view(boost::string_view(string_buffer_.data() + offset, str.size()));
                offset += str.size();
            }
        }
    }
}

#endif
