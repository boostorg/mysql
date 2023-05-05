//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPING_META_CHECK_CONTEXT_HPP
#define BOOST_MYSQL_DETAIL_TYPING_META_CHECK_CONTEXT_HPP

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/typing/cpp2db_map.hpp>

#include <memory>
#include <sstream>

namespace boost {
namespace mysql {
namespace detail {

inline const char* column_type_to_str(const metadata& meta) noexcept
{
    switch (meta.type())
    {
    case column_type::tinyint: return meta.is_unsigned() ? "TINYINT UNSIGNED" : "TINYINT";
    case column_type::smallint: return meta.is_unsigned() ? "SMALLINT UNSIGNED" : "SMALLINT";
    case column_type::mediumint: return meta.is_unsigned() ? "MEDIUMINT UNSIGNED" : "MEDIUMINT";
    case column_type::int_: return meta.is_unsigned() ? "INT UNSIGNED" : "INT";
    case column_type::bigint: return meta.is_unsigned() ? "BIGINT UNSIGNED" : "BIGINT";
    case column_type::float_: return "FLOAT";
    case column_type::double_: return "DOUBLE";
    case column_type::decimal: return "DECIMAL";
    case column_type::bit: return "BIT";
    case column_type::year: return "YEAR";
    case column_type::time: return "TIME";
    case column_type::date: return "DATE";
    case column_type::datetime: return "DATETIME";
    case column_type::timestamp: return "TIMESTAMP";
    case column_type::char_: return "CHAR";
    case column_type::varchar: return "VARCHAR";
    case column_type::binary: return "BINARY";
    case column_type::varbinary: return "VARBINARY";
    case column_type::text: return "TEXT";
    case column_type::blob: return "BLOB";
    case column_type::enum_: return "ENUM";
    case column_type::set: return "SET";
    case column_type::json: return "JSON";
    case column_type::geometry: return "GEOMETRY";
    default: return "<unknown column type>";
    }
}

class meta_check_context
{
    std::unique_ptr<std::ostringstream> errors_;
    std::size_t current_index_{};
    const_cpp2db_t pos_map_;
    name_table_t name_table_;
    metadata_collection_view meta_{};
    bool nullability_checked_{};

    std::ostringstream& add_error()
    {
        if (!errors_)
            errors_.reset(new std::ostringstream);
        else
            *errors_ << '\n';
        return *errors_;
    }

    void insert_field_name(std::ostringstream& os)
    {
        if (has_field_names(name_table_))
            os << "'" << name_table_[current_index_] << "'";
        else
            os << "in position " << current_index_;
    }

public:
    meta_check_context(
        const_cpp2db_t pos_map,
        name_table_t name_table,
        metadata_collection_view meta
    ) noexcept
        : pos_map_(pos_map), name_table_(name_table), meta_(meta)
    {
    }

    // Accessors
    const metadata& current_meta() const noexcept { return map_metadata(pos_map_, current_index_, meta_); }
    bool is_current_field_absent() const noexcept { return pos_map_[current_index_] == pos_absent; }

    // Iteration
    void advance() noexcept
    {
        nullability_checked_ = false;
        ++current_index_;
    }

    // Nullability
    void set_nullability_checked() noexcept { nullability_checked_ = true; }
    bool nullability_checked() const noexcept { return nullability_checked_; }

    // Error reporting
    void add_field_absent_error()
    {
        auto& stream = add_error();
        stream << "Field ";
        insert_field_name(stream);
        if (has_field_names(name_table_))
        {
            stream << " is not present in the data returned by the server";
        }
        else
        {
            stream << " can't be mapped: there are more fields in your C++ data type than in your query";
        }
    }

    void add_type_mismatch_error(const char* cpp_type_name)
    {
        auto& stream = add_error();
        stream << "Incompatible types for field ";
        insert_field_name(stream);
        stream << ": C++ type '" << cpp_type_name << "' is not compatible with DB type '"
               << column_type_to_str(current_meta()) << "'";
    }

    void add_nullability_error()
    {
        auto& stream = add_error();
        stream << "NULL checks failed for field ";
        insert_field_name(stream);
        stream << ": the database type may be NULL, but the C++ type cannot. Use std::optional<T> or "
                  "boost::optional<T>";
    }

    error_code check_errors(diagnostics& diag) const
    {
        if (errors_ != nullptr)
        {
            diagnostics_access::assign(diag, errors_->str(), false);
            return client_errc::metadata_check_failed;
        }
        return error_code();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
