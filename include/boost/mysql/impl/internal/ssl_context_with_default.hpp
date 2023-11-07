//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SSL_CONTEXT_WITH_DEFAULT_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SSL_CONTEXT_WITH_DEFAULT_HPP

#include <boost/asio/ssl/context.hpp>
#include <boost/variant2/variant.hpp>

namespace boost {
namespace mysql {
namespace detail {

class ssl_context_with_default
{
    variant2::variant<asio::ssl::context*, asio::ssl::context> impl_;

public:
    ssl_context_with_default(asio::ssl::context* ctx) noexcept : impl_(ctx) {}

    asio::ssl::context& get()
    {
        // Do we have a default context already created?
        auto* default_ctx = variant2::get_if<asio::ssl::context>(&impl_);
        if (default_ctx)
            return *default_ctx;

        // Do we have an external context provided by the user?
        BOOST_ASSERT(variant2::holds_alternative<asio::ssl::context*>(impl_));
        auto* external_ctx = variant2::unsafe_get<0>(impl_);
        if (external_ctx)
            return *external_ctx;

        // Create a default context and return it.
        // As of MySQL 5.7.35, support for previous TLS versions is deprecated,
        // so this is a secure default. User can override it if they want
        return impl_.emplace<asio::ssl::context>(asio::ssl::context::tlsv12_client);
    }

    // Exposed for the sake of testing
    const asio::ssl::context* get_ptr() const noexcept
    {
        if (const auto* res = variant2::get_if<asio::ssl::context>(&impl_))
            return res;
        else
            return variant2::unsafe_get<0>(impl_);
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
