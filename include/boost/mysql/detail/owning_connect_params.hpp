//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_OWNING_CONNECT_PARAMS_HPP
#define BOOST_MYSQL_DETAIL_OWNING_CONNECT_PARAMS_HPP

#include <boost/mysql/address_type.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/any_address_view.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace boost {
namespace mysql {
namespace detail {

class owning_connect_params
{
    static constexpr std::size_t sbo_size = 200u;

    address_type addr_type_{address_type::tcp_address};
    std::size_t address_size_{};
    unsigned short port_{};
    std::size_t username_size_{};
    std::size_t password_size_{};
    std::size_t database_size_{};
    std::uint16_t connection_collation_{};
    ssl_mode ssl_{ssl_mode::require};
    bool multi_queries_{};
    std::array<char, sbo_size> string_buff_;
    std::unique_ptr<char[]> extra_space_;

    std::size_t address_offset() const noexcept { return 0; }
    std::size_t username_offset() const noexcept { return address_size_; }
    std::size_t password_offset() const noexcept { return username_offset() + username_size_; }
    std::size_t database_offset() const noexcept { return password_offset() + password_size_; }

    string_view get_string(std::size_t offset, std::size_t size) const noexcept
    {
        const char* base = extra_space_ ? extra_space_.get() : string_buff_.data();
        return string_view(base + offset, size);
    }

    static std::size_t total_string_size(const connect_params& p) noexcept
    {
        const auto& impl = access::get_impl(p);
        return impl.address.size() + 1 + impl.username.size() + impl.password.size() + impl.database.size();
    }

public:
    owning_connect_params(const connect_params& params)
        : addr_type_(params.addr_type()),
          address_size_(access::get_impl(params).address.size() + 1),  // NULL terminator, for UNIX sockets
          port_(access::get_impl(params).port),
          username_size_(params.username().size()),
          password_size_(params.password().size()),
          database_size_(params.database().size()),
          connection_collation_(params.connection_collation()),
          ssl_(params.ssl()),
          multi_queries_(params.multi_queries())
    {
        // Allocate space for strings if required
        auto total_size = total_string_size(params);
        char* it = string_buff_.data();
        if (total_size > sbo_size)
        {
            extra_space_.reset(new char[total_size]);
            it = extra_space_.get();
        }

        // Copy strings
        if (address_size_ > 1u)
        {
            std::memcpy(it, access::get_impl(params).address.data(), address_size_ - 1);
            it += address_size_;
            *it++ = '\0';
        }
        if (username_size_)
        {
            std::memcpy(it, params.username().data(), username_size_);
            it += username_size_;
        }
        if (password_size_)
        {
            std::memcpy(it, params.password().data(), password_size_);
            it += password_size_;
        }
        if (database_size_)
        {
            std::memcpy(it, params.database().data(), database_size_);
            it += database_size_;
        }
    }

    any_address_view address() const noexcept
    {
        return {
            addr_type_,
            get_string(address_offset(), address_size_),
            port_,
        };
    }

    handshake_params hparams() const noexcept
    {
        return handshake_params(
            get_string(username_offset(), username_size_),
            get_string(password_offset(), password_size_),
            get_string(database_offset(), database_size_),
            connection_collation_,
            ssl_,
            multi_queries_
        );
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
