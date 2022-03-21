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

namespace boost {
namespace mysql {

/**
 * \brief Holds [link mysql.resultsets.metadata metadata] about a field in a SQL query.
 * \details All strings point into externally owned memory. The object
 * will be valid while the parent object is alive
 * (typically, a [reflink resultset] object).
 */
class field_metadata
{
    detail::column_definition_packet msg_;
    mutable field_type field_type_ { field_type::_not_computed };

    bool flag_set(std::uint16_t flag) const noexcept { return msg_.flags & flag; }
public:
    /**
     * \brief Default constructor.
     * \details The constructed metadata object will have undefined
     * values for all of its members.
     */
    field_metadata() = default;

#ifndef BOOST_MYSQL_DOXYGEN
    // Private, do not use.
    field_metadata(const detail::column_definition_packet& msg) noexcept: msg_(msg) {};
#endif

    /// Returns the name of the database (schema) the field belongs to.
    boost::string_view database() const noexcept { return msg_.schema.value; }

    /**
     * \brief Returns the name of the virtual table the field belongs to.
     * \details If the table was aliased, this will be the name of the alias
     * (e.g. in `"SELECT * FROM employees emp"`, `table()` will be `"emp"`).
     */
    boost::string_view table() const noexcept { return msg_.table.value; }

    /**
     * \brief Returns the name of the physical table the field belongs to.
     * \details E.g. in `"SELECT * FROM employees emp"`,
     * `original_table()` will be `"employees"`.
     */
    boost::string_view original_table() const noexcept { return msg_.org_table.value; }

    /**
     * \brief Returns the actual name of the field.
     * \details If the field was aliased, this will be the name of the alias
     * (e.g. in `"SELECT id AS employee_id FROM employees"`,
     * `field_name()` will be `"employee_id"`).
     */
    boost::string_view field_name() const noexcept { return msg_.name.value; }

    /**
     * \brief Returns the original (physical) name of the field.
     * \details E.g. in `"SELECT id AS employee_id FROM employees"`,
     * `original_field_name()` will be `"id"`.
     */
    boost::string_view original_field_name() const noexcept { return msg_.org_name.value; }

    /// Returns the character set ([reflink collation]) for the column.
    collation character_set() const noexcept { return msg_.character_set; }

    /// Returns the maximum length of the field.
    unsigned column_length() const noexcept { return msg_.column_length; }

#ifndef BOOST_MYSQL_DOXYGEN
    // Private, do not use
    detail::protocol_field_type protocol_type() const noexcept { return msg_.type; }
#endif

    /// Returns the type of the field (see [reflink field_type] for more info).
    field_type type() const noexcept;

    /// Returns the number of decimals of the field.
    unsigned decimals() const noexcept { return msg_.decimals; }

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

namespace detail {

class resultset_metadata
{
    std::vector<bytestring> buffers_;
    std::vector<field_metadata> fields_;
public:
    resultset_metadata() = default;
    resultset_metadata(std::vector<bytestring>&& buffers, std::vector<field_metadata>&& fields):
        buffers_(std::move(buffers)), fields_(std::move(fields)) {};
    resultset_metadata(const resultset_metadata&) = delete;
    resultset_metadata(resultset_metadata&&) = default;
    resultset_metadata& operator=(const resultset_metadata&) = delete;
    resultset_metadata& operator=(resultset_metadata&&) = default;
    ~resultset_metadata() = default;
    const std::vector<field_metadata>& fields() const noexcept { return fields_; }
};

} // detail
} // mysql
} // boost

#include <boost/mysql/impl/metadata.ipp>

#endif
