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

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/constants.hpp>
#include <boost/mysql/impl/metadata.hpp>

namespace boost {
namespace mysql {
namespace test {

class meta_builder
{
    detail::column_definition_packet pack_{};
    bool coltype_set_{false};
    column_type type_{};

public:
    meta_builder(detail::protocol_field_type t = detail::protocol_field_type::enum_)
    {
        pack_.type = t;
        pack_.character_set = 33;  // utf8_general_ci
    }
    meta_builder& flags(std::uint16_t v) noexcept
    {
        pack_.flags = v;
        return *this;
    }
    meta_builder& decimals(std::uint8_t v) noexcept
    {
        pack_.decimals = v;
        return *this;
    }
    meta_builder& collation(std::uint16_t v) noexcept
    {
        pack_.character_set = v;
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
        if (v)
            pack_.flags |= detail::column_flags::unsigned_;
        else
            pack_.flags &= ~detail::column_flags::unsigned_;
        return *this;
    }
    metadata build()
    {
        auto res = detail::metadata_access::construct(pack_, false);
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
