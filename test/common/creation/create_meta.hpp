//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_META_HPP
#define BOOST_MYSQL_TEST_COMMON_CREATION_CREATE_META_HPP

#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_mode.hpp>

#include <boost/mysql/detail/protocol/common_messages.hpp>

namespace boost {
namespace mysql {
namespace test {

class meta_builder
{
    detail::column_definition_packet pack_{};

public:
    meta_builder(detail::protocol_field_type t)
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
    metadata build() { return detail::metadata_access::construct(pack_, false); }
};

inline metadata create_meta(detail::protocol_field_type t) { return meta_builder(t).build(); }

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
