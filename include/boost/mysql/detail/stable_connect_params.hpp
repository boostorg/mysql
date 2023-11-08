//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_STABLE_CONNECT_PARAMS_HPP
#define BOOST_MYSQL_DETAIL_STABLE_CONNECT_PARAMS_HPP

#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/any_address.hpp>
#include <boost/mysql/detail/config.hpp>

#include <memory>

namespace boost {
namespace mysql {
namespace detail {

struct stable_connect_params
{
    any_address address;
    handshake_params hparams;
    std::unique_ptr<char[]> string_buffer;
};

BOOST_MYSQL_DECL
stable_connect_params make_stable(const connect_params& input);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/stable_connect_params.ipp>
#endif

#endif
