//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>

#include <boost/mysql/detail/auxiliar/access_fwd.hpp>
#include <boost/mysql/detail/row_impl.hpp>
#include <boost/mysql/detail/throw_on_error_loc.hpp>

#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>

#include "protocol/protocol.hpp"

using namespace boost::mysql;

// metadata
boost::mysql::metadata::metadata(const detail::coldef_view& msg, bool copy_strings)
    : schema_(copy_strings ? msg.database : string_view()),
      table_(copy_strings ? msg.table : string_view()),
      org_table_(copy_strings ? msg.org_table : string_view()),
      name_(copy_strings ? msg.column_name : string_view()),
      org_name_(copy_strings ? msg.org_column_name : string_view()),
      character_set_(msg.collation_id),
      column_length_(msg.column_length),
      type_(msg.type),
      flags_(msg.flags),
      decimals_(msg.decimals)
{
}

// row_impl
static std::size_t get_string_size(field_view f) noexcept
{
    switch (f.kind())
    {
    case field_kind::string: return f.get_string().size();
    case field_kind::blob: return f.get_blob().size();
    default: return 0;
    }
}

static unsigned char* copy_string(unsigned char* buffer_it, field_view& f) noexcept
{
    auto str = f.get_string();
    if (!str.empty())
    {
        std::memcpy(buffer_it, str.data(), str.size());
        f = field_view(string_view(reinterpret_cast<const char*>(buffer_it), str.size()));
        buffer_it += str.size();
    }
    return buffer_it;
}

static unsigned char* copy_blob(unsigned char* buffer_it, field_view& f) noexcept
{
    auto b = f.get_blob();
    if (!b.empty())
    {
        std::memcpy(buffer_it, b.data(), b.size());
        f = field_view(blob_view(buffer_it, b.size()));
        buffer_it += b.size();
    }
    return buffer_it;
}

static std::size_t copy_string_as_offset(
    unsigned char* buffer_first,
    std::size_t offset,
    field_view& f
) noexcept
{
    auto str = f.get_string();
    if (!str.empty())
    {
        std::memcpy(buffer_first + offset, str.data(), str.size());
        f = detail::impl_access::construct<field_view>(detail::string_view_offset(offset, str.size()), false);
        return str.size();
    }
    return 0;
}

static std::size_t copy_blob_as_offset(
    unsigned char* buffer_first,
    std::size_t offset,
    field_view& f
) noexcept
{
    auto str = f.get_blob();
    if (!str.empty())
    {
        std::memcpy(buffer_first + offset, str.data(), str.size());
        f = detail::impl_access::construct<field_view>(detail::string_view_offset(offset, str.size()), true);
        return str.size();
    }
    return 0;
}

static void copy_strings(std::vector<field_view>& fields, std::vector<unsigned char>& string_buffer)
{
    // Calculate the required size for the new strings
    std::size_t size = 0;
    for (auto f : fields)
    {
        size += get_string_size(f);
    }

    // Make space. The previous fields should be in offset form
    string_buffer.resize(string_buffer.size() + size);

    // Copy strings and blobs
    unsigned char* buffer_it = string_buffer.data();
    for (auto& f : fields)
    {
        switch (f.kind())
        {
        case field_kind::string: buffer_it = copy_string(buffer_it, f); break;
        case field_kind::blob: buffer_it = copy_blob(buffer_it, f); break;
        default: break;
        }
    }
    BOOST_ASSERT(buffer_it == string_buffer_.data() + size);
}

boost::mysql::detail::row_impl::row_impl(const field_view* fields, std::size_t size)
    : fields_(fields, fields + size)
{
    copy_strings(fields_, string_buffer_);
}

boost::mysql::detail::row_impl::row_impl(const row_impl& rhs) : fields_(rhs.fields_)
{
    copy_strings(fields_, string_buffer_);
}

boost::mysql::detail::row_impl& boost::mysql::detail::row_impl::operator=(const row_impl& rhs)
{
    assign(rhs.fields_.data(), rhs.fields_.size());
    return *this;
}

void boost::mysql::detail::row_impl::assign(const field_view* fields, std::size_t size)
{
    // Protect against self-assignment. This is valid as long as we
    // don't implement sub-range operators (e.g. row_view[2:4])
    if (fields_.data() == fields)
    {
        BOOST_ASSERT(fields_.size() == size);
    }
    else
    {
        fields_.assign(fields, fields + size);
        string_buffer_.clear();
        copy_strings(fields_, string_buffer_);
    }
}

void boost::mysql::detail::row_impl::copy_strings_as_offsets(std::size_t first, std::size_t num_fields)
{
    // Preconditions
    BOOST_ASSERT(first <= fields_.size());
    BOOST_ASSERT(first + num_fields <= fields_.size());

    // Calculate the required size for the new strings
    std::size_t size = 0;
    for (std::size_t i = first; i < first + num_fields; ++i)
    {
        size += get_string_size(fields_[i]);
    }

    // Make space. The previous fields should be in offset form
    std::size_t old_string_buffer_size = string_buffer_.size();
    string_buffer_.resize(old_string_buffer_size + size);

    // Copy strings and blobs
    std::size_t offset = old_string_buffer_size;
    for (std::size_t i = first; i < first + num_fields; ++i)
    {
        auto& f = fields_[i];
        switch (f.kind())
        {
        case field_kind::string: offset += copy_string_as_offset(string_buffer_.data(), offset, f); break;
        case field_kind::blob: offset += copy_blob_as_offset(string_buffer_.data(), offset, f); break;
        default: break;
        }
    }
    BOOST_ASSERT(offset == string_buffer_.size());
}

// throw_on_error_loc
void boost::mysql::detail::throw_on_error_loc(
    error_code err,
    const diagnostics& diag,
    const boost::source_location& loc

)
{
    if (err)
    {
        ::boost::throw_exception(error_with_diagnostics(err, diag), loc);
    }
}