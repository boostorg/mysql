//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/field.hpp>
#include <boost/mysql/field_kind.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/resultset.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/row_impl.hpp>
#include <boost/mysql/detail/throw_on_error_loc.hpp>

#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>

#include <cstdio>
#include <ostream>

#include "protocol/protocol.hpp"

using namespace boost::mysql;

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
        f = detail::access::construct<field_view>(detail::string_view_offset(offset, str.size()), false);
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
        f = detail::access::construct<field_view>(detail::string_view_offset(offset, str.size()), true);
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
    BOOST_ASSERT(buffer_it == string_buffer.data() + size);
}

static field_view offset_to_string_view(field_view fv, const std::uint8_t* buffer_first) noexcept
{
    auto& impl = detail::access::get_impl(fv);
    if (impl.is_string_offset())
    {
        return field_view(string_view(
            reinterpret_cast<const char*>(buffer_first) + impl.repr.sv_offset_.offset,
            impl.repr.sv_offset_.size
        ));
    }
    else if (impl.is_blob_offset())
    {
        return field_view(blob_view(buffer_first + impl.repr.sv_offset_.offset, impl.repr.sv_offset_.size));
    }
    else
    {
        return fv;
    }
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

void boost::mysql::detail::row_impl::offsets_to_string_views()
{
    for (auto& f : fields_)
        f = offset_to_string_view(f, string_buffer_.data());
}

// field_view
static std::ostream& print_blob(std::ostream& os, blob_view value)
{
    if (value.empty())
        return os << "{}";

    char buffer[16]{};

    os << "{ ";
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (i != 0)
            os << ", ";
        unsigned byte = value[i];
        std::snprintf(buffer, sizeof(buffer), "0x%02x", byte);
        os << buffer;
    }
    os << " }";
    return os;
}

static std::ostream& print_time(std::ostream& os, const boost::mysql::time& value)
{
    // Worst-case output is 26 chars, extra space just in case
    char buffer[64]{};

    using namespace std::chrono;
    const char* sign = value < microseconds(0) ? "-" : "";
    auto num_micros = value % seconds(1);
    auto num_secs = duration_cast<seconds>(value % minutes(1) - num_micros);
    auto num_mins = duration_cast<minutes>(value % hours(1) - num_secs);
    auto num_hours = duration_cast<hours>(value - num_mins);

    snprintf(
        buffer,
        sizeof(buffer),
        "%s%02d:%02u:%02u.%06u",
        sign,
        static_cast<int>(std::abs(num_hours.count())),
        static_cast<unsigned>(std::abs(num_mins.count())),
        static_cast<unsigned>(std::abs(num_secs.count())),
        static_cast<unsigned>(std::abs(num_micros.count()))
    );

    os << buffer;
    return os;
}

std::ostream& boost::mysql::operator<<(std::ostream& os, const field_view& value)
{
    // Make operator<< work for detail::string_view_offset types
    if (value.impl_.is_string_offset() || value.impl_.is_blob_offset())
    {
        return os << "<sv_offset>";
    }

    switch (value.kind())
    {
    case field_kind::null: return os << "<NULL>";
    case field_kind::int64: return os << value.get_int64();
    case field_kind::uint64: return os << value.get_uint64();
    case field_kind::string: return os << value.get_string();
    case field_kind::blob: return print_blob(os, value.get_blob());
    case field_kind::float_: return os << value.get_float();
    case field_kind::double_: return os << value.get_double();
    case field_kind::date: return os << value.get_date();
    case field_kind::datetime: return os << value.get_datetime();
    case field_kind::time: return print_time(os, value.get_time());
    default: BOOST_ASSERT(false); return os;
    }
}

// column_type
std::ostream& boost::mysql::operator<<(std::ostream& os, column_type t)
{
    switch (t)
    {
    case column_type::tinyint: return os << "tinyint";
    case column_type::smallint: return os << "smallint";
    case column_type::mediumint: return os << "mediumint";
    case column_type::int_: return os << "int_";
    case column_type::bigint: return os << "bigint";
    case column_type::float_: return os << "float_";
    case column_type::double_: return os << "double_";
    case column_type::decimal: return os << "decimal";
    case column_type::bit: return os << "bit";
    case column_type::year: return os << "year";
    case column_type::time: return os << "time";
    case column_type::date: return os << "date";
    case column_type::datetime: return os << "datetime";
    case column_type::timestamp: return os << "timestamp";
    case column_type::char_: return os << "char_";
    case column_type::varchar: return os << "varchar";
    case column_type::binary: return os << "binary";
    case column_type::varbinary: return os << "varbinary";
    case column_type::text: return os << "text";
    case column_type::blob: return os << "blob";
    case column_type::enum_: return os << "enum_";
    case column_type::set: return os << "set";
    case column_type::json: return os << "json";
    case column_type::geometry: return os << "geometry";
    default: return os << "<unknown column type>";
    }
}

// date
std::ostream& boost::mysql::operator<<(std::ostream& os, const date& value)
{
    // Worst-case output is 14 chars, extra space just in case
    char buffer[32]{};
    snprintf(
        buffer,
        sizeof(buffer),
        "%04u-%02u-%02u",
        static_cast<unsigned>(value.year()),
        static_cast<unsigned>(value.month()),
        static_cast<unsigned>(value.day())
    );
    os << buffer;
    return os;
}

// datetime
std::ostream& boost::mysql::operator<<(std::ostream& os, const datetime& value)
{
    // Worst-case output is 37 chars, extra space just in case
    char buffer[64]{};
    snprintf(
        buffer,
        sizeof(buffer),
        "%04u-%02u-%02u %02d:%02u:%02u.%06u",
        static_cast<unsigned>(value.year()),
        static_cast<unsigned>(value.month()),
        static_cast<unsigned>(value.day()),
        static_cast<unsigned>(value.hour()),
        static_cast<unsigned>(value.minute()),
        static_cast<unsigned>(value.second()),
        static_cast<unsigned>(value.microsecond())
    );
    os << buffer;
    return os;
}

// field_kind
std::ostream& boost::mysql::operator<<(std::ostream& os, boost::mysql::field_kind v)
{
    switch (v)
    {
    case field_kind::null: return os << "null";
    case field_kind::int64: return os << "int64";
    case field_kind::uint64: return os << "uint64";
    case field_kind::string: return os << "string";
    case field_kind::float_: return os << "float_";
    case field_kind::double_: return os << "double_";
    case field_kind::date: return os << "date";
    case field_kind::datetime: return os << "datetime";
    case field_kind::time: return os << "time";
    default: return os << "<invalid>";
    }
}

// field
static blob to_blob(blob_view v) { return blob(v.data(), v.data() + v.size()); }

void boost::mysql::field::from_view(const field_view& fv)
{
    switch (fv.kind())
    {
    case field_kind::null: repr_.data.emplace<detail::field_impl::null_t>(); break;
    case field_kind::int64: repr_.data.emplace<std::int64_t>(fv.get_int64()); break;
    case field_kind::uint64: repr_.data.emplace<std::uint64_t>(fv.get_uint64()); break;
    case field_kind::string: repr_.data.emplace<std::string>(fv.get_string()); break;
    case field_kind::blob: repr_.data.emplace<blob>(to_blob(fv.get_blob())); break;
    case field_kind::float_: repr_.data.emplace<float>(fv.get_float()); break;
    case field_kind::double_: repr_.data.emplace<double>(fv.get_double()); break;
    case field_kind::date: repr_.data.emplace<date>(fv.get_date()); break;
    case field_kind::datetime: repr_.data.emplace<datetime>(fv.get_datetime()); break;
    case field_kind::time: repr_.data.emplace<time>(fv.get_time()); break;
    }
}

std::ostream& boost::mysql::operator<<(std::ostream& os, const field& value)
{
    return os << field_view(value);
}

// resultset
void boost::mysql::resultset::assign(resultset_view v)
{
    has_value_ = v.has_value();
    if (has_value_)
    {
        meta_.assign(v.meta().begin(), v.meta().end());
        rws_ = v.rows();
        affected_rows_ = v.affected_rows();
        last_insert_id_ = v.last_insert_id();
        warnings_ = v.warning_count();
        info_.assign(v.info().begin(), v.info().end());
        is_out_params_ = v.is_out_params();
    }
    else
    {
        meta_.clear();
        rws_ = ::boost::mysql::rows();
        affected_rows_ = 0;
        last_insert_id_ = 0;
        warnings_ = 0;
        info_.clear();
        is_out_params_ = false;
    }
}

// row_view
bool boost::mysql::operator==(const row_view& lhs, const row_view& rhs) noexcept
{
    if (lhs.size() != rhs.size())
        return false;
    for (std::size_t i = 0; i < lhs.size(); ++i)
    {
        if (lhs[i] != rhs[i])
            return false;
    }
    return true;
}

// rows_view
bool boost::mysql::rows_view::operator==(const rows_view& rhs) const noexcept
{
    if (num_fields_ != rhs.num_fields_ || num_columns_ != rhs.num_columns_)
        return false;
    for (std::size_t i = 0; i < num_fields_; ++i)
    {
        if (fields_[i] != rhs.fields_[i])
            return false;
    }
    return true;
}