//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_META_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_META_HPP

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>

namespace boost {
namespace mysql {
namespace test {

class coldef_builder
{
    detail::column_definition_packet pack_{};

public:
    coldef_builder() = default;
    coldef_builder& type(detail::protocol_field_type v) noexcept
    {
        pack_.type = v;
        return *this;
    }
    coldef_builder& flags(std::uint16_t v) noexcept
    {
        pack_.flags = v;
        return *this;
    }
    coldef_builder& decimals(std::uint8_t v) noexcept
    {
        pack_.decimals = v;
        return *this;
    }
    coldef_builder& collation(std::uint16_t v) noexcept
    {
        pack_.character_set = v;
        return *this;
    }
    coldef_builder& unsigned_flag(bool v) noexcept
    {
        if (v)
            pack_.flags |= detail::column_flags::unsigned_;
        else
            pack_.flags &= ~detail::column_flags::unsigned_;
        return *this;
    }
    coldef_builder& nullable(bool v) noexcept
    {
        if (v)
            pack_.flags &= ~detail::column_flags::not_null;
        else
            pack_.flags |= detail::column_flags::not_null;
        return *this;
    }
    coldef_builder& name(string_view v) noexcept
    {
        pack_.name.value = v;
        return *this;
    }
    detail::column_definition_packet build() { return pack_; }
};

class meta_builder
{
    coldef_builder b_;
    bool coltype_set_{false};
    column_type type_{};

public:
    meta_builder(detail::protocol_field_type t = detail::protocol_field_type::enum_)
    {
        b_.type(t).collation(33);  // utf8_general_ci
    }
    meta_builder& flags(std::uint16_t v) noexcept
    {
        b_.flags(v);
        return *this;
    }
    meta_builder& decimals(std::uint8_t v) noexcept
    {
        b_.decimals(v);
        return *this;
    }
    meta_builder& collation(std::uint16_t v) noexcept
    {
        b_.collation(v);
        return *this;
    }
    meta_builder& type(column_type v) noexcept
    {
        coltype_set_ = true;
        type_ = v;
        return *this;
    }
    meta_builder& unsigned_flag(bool v) noexcept
    {
        b_.unsigned_flag(v);
        return *this;
    }
    meta_builder& nullable(bool v) noexcept
    {
        b_.nullable(v);
        return *this;
    }
    meta_builder& name(string_view v) noexcept
    {
        b_.name(v);
        return *this;
    }
    metadata build()
    {
        auto res = detail::impl_access::construct<metadata>(b_.build(), true);
        if (coltype_set_)
            detail::metadata_access::set_type(res, type_);
        return res;
    }
};

inline metadata create_meta(detail::protocol_field_type t) { return meta_builder(t).build(); }

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
