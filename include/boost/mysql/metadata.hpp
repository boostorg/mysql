//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_METADATA_HPP
#define BOOST_MYSQL_METADATA_HPP

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/field_type.hpp>
#include <boost/utility/string_view_fwd.hpp>
#include <string>

namespace boost {
namespace mysql {

/**
 * \brief Holds [link mysql.resultsets.metadata metadata] about a field in a SQL query.
 * \details All strings point into externally owned memory. The object
 * will be valid while the parent object is alive
 * (typically, a [reflink resultset_base] object).
 */
class metadata
{
    std::string schema_;
    std::string table_; // virtual table
    std::string org_table_; // physical table
    std::string name_; // virtual column name
    std::string org_name_; // physical column name
    collation character_set_;
    std::uint32_t column_length_; // maximum length of the field
    detail::protocol_field_type type_; // type of the column as defined in enum_field_types
    std::uint16_t flags_; // Flags as defined in Column Definition Flags
    std::uint8_t decimals_; // max shown decimal digits. 0x00 for int/static strings; 0x1f for dynamic strings, double, float
    mutable field_type field_type_ { field_type::_not_computed };

    bool flag_set(std::uint16_t flag) const noexcept { return flags_ & flag; }
public:
    /**
     * \brief Default constructor.
     * \details The constructed metadata object will have undefined
     * values for all of its members.
     */
    metadata() = default;

#ifndef BOOST_MYSQL_DOXYGEN
    // Private, do not use.
    // TODO: hide this
    metadata(const detail::column_definition_packet& msg, bool copy_strings) :
        schema_(copy_strings ? msg.schema.value : boost::string_view()),
        table_(copy_strings ? msg.table.value : boost::string_view()),
        org_table_(copy_strings ? msg.org_table.value : boost::string_view()),
        name_(copy_strings ? msg.name.value : boost::string_view()),
        org_name_(copy_strings ? msg.org_name.value : boost::string_view()),
        character_set_(msg.character_set),
        column_length_(msg.column_length),
        type_(msg.type),
        flags_(msg.flags),
        decimals_(msg.decimals)
    {

    }
#endif

    /// Returns the name of the database (schema) the field belongs to.
    boost::string_view database() const noexcept { return schema_; }

    /**
     * \brief Returns the name of the virtual table the field belongs to.
     * \details If the table was aliased, this will be the name of the alias
     * (e.g. in `"SELECT * FROM employees emp"`, `table()` will be `"emp"`).
     */
    boost::string_view table() const noexcept { return table_; }

    /**
     * \brief Returns the name of the physical table the field belongs to.
     * \details E.g. in `"SELECT * FROM employees emp"`,
     * `original_table()` will be `"employees"`.
     */
    boost::string_view original_table() const noexcept { return org_table_; }

    /**
     * \brief Returns the actual name of the field.
     * \details If the field was aliased, this will be the name of the alias
     * (e.g. in `"SELECT id AS employee_id FROM employees"`,
     * `field_name()` will be `"employee_id"`).
     */
    boost::string_view field_name() const noexcept { return name_; }

    /**
     * \brief Returns the original (physical) name of the field.
     * \details E.g. in `"SELECT id AS employee_id FROM employees"`,
     * `original_field_name()` will be `"id"`.
     */
    boost::string_view original_field_name() const noexcept { return org_name_; }

    /// Returns the character set ([reflink collation]) for the column.
    collation character_set() const noexcept { return character_set_; }

    /// Returns the maximum length of the field.
    unsigned column_length() const noexcept { return column_length_; }

#ifndef BOOST_MYSQL_DOXYGEN
    // Private, do not use. TODO: hide this
    detail::protocol_field_type protocol_type() const noexcept { return type_; }
#endif

    /// Returns the type of the field (see [reflink field_type] for more info).
    field_type type() const noexcept;

    /// Returns the number of decimals of the field.
    unsigned decimals() const noexcept { return decimals_; }

    /// Returns `true` if the field is not allowed to be NULL, `false` if it is nullable.
    bool is_not_null() const noexcept { return flag_set(detail::column_flags::not_null); }

    /// Returns `true` if the field is part of a `PRIMARY KEY`.
    bool is_primary_key() const noexcept { return flag_set(detail::column_flags::pri_key); }

    /// Returns `true` if the field is part of a `UNIQUE KEY` (but not a `PRIMARY KEY`).
    bool is_unique_key() const noexcept { return flag_set(detail::column_flags::unique_key); }

    /// Returns `true` if the field is part of a `KEY` (but not a `UNIQUE KEY` or `PRIMARY KEY`).
    bool is_multiple_key() const noexcept { return flag_set(detail::column_flags::multiple_key); }

    /// Returns `true` if the field has no sign (is `UNSIGNED`).
    bool is_unsigned() const noexcept { return flag_set(detail::column_flags::unsigned_); }

    /// Returns `true` if the field is defined as `ZEROFILL` (padded to its maximum length by zeros).
    bool is_zerofill() const noexcept { return flag_set(detail::column_flags::zerofill); }

    /// Returns `true` if the field is defined as `AUTO_INCREMENT`.
    bool is_auto_increment() const noexcept { return flag_set(detail::column_flags::auto_increment); }

    /// Returns `true` if the field does not have a default value.
    bool has_no_default_value() const noexcept { return flag_set(detail::column_flags::no_default_value); }

    /// Returns `true` if the field is defined as `ON UPDATE CURRENT_TIMESTAMP`.
    bool is_set_to_now_on_update() const noexcept { return flag_set(detail::column_flags::on_update_now); }
};

} // mysql
} // boost

#include <boost/mysql/impl/metadata.ipp>

#endif
