//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_ASYNC_HELPERS_HPP
#define BOOST_MYSQL_DETAIL_ASYNC_HELPERS_HPP

#include <boost/asio/any_io_executor.hpp>

namespace boost {
namespace mysql {
namespace detail {

// Base class for initiation objects. Includes a bound executor, so they're compatible
// with asio::cancel_after and similar
struct initiation_base
{
    asio::any_io_executor ex;

    initiation_base(asio::any_io_executor ex) noexcept : ex(std::move(ex)) {}

    using executor_type = asio::any_io_executor;
    const executor_type& get_executor() const noexcept { return ex; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
