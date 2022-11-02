//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_USE_VIEWS_HPP
#define BOOST_MYSQL_USE_VIEWS_HPP

namespace boost {
namespace mysql {

/**
 * \brief Placeholder type to indicate that a function should return non-owning views.
 */
struct use_views_t
{
};

/// Placeholder to indicate that a function should return non-owning views.
constexpr use_views_t use_views{};

}  // namespace mysql
}  // namespace boost

#endif
