//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_OWNING_CONNECT_PARAMS_HPP
#define BOOST_MYSQL_DETAIL_OWNING_CONNECT_PARAMS_HPP

#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/any_address.hpp>

#include <cstddef>
#include <memory>

namespace boost {
namespace mysql {
namespace detail {

struct owning_connect_params
{
    any_address address;
    handshake_params hparams;
    std::unique_ptr<char[]> string_buffer;

    static owning_connect_params create(connect_params input)
    {
        auto& impl = access::get_impl(input);

        // Calculate required space
        std::size_t required_size = impl.address.size() + impl.username.size() + impl.password.size() +
                                    impl.database.size();

        // Allocate space for strings
        auto ptr = std::make_unique<char[]>(required_size);

        // Copy them to the new buffer, and update the string views
        char* it = ptr.get();
        for (string_view* elm : {&impl.address, &impl.username, &impl.password, &impl.database})
        {
            if (!elm->empty())
            {
                std::memcpy(it, elm->data(), elm->size());
                *elm = string_view(it, elm->size());
                it += elm->size();
            }
        }

        return {impl.to_address(), impl.to_handshake_params(), std::move(ptr)};
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
