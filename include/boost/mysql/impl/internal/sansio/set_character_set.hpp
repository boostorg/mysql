//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_SET_CHARACTER_SET_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_SET_CHARACTER_SET_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/algo_params.hpp>

#include <boost/mysql/impl/internal/protocol/protocol.hpp>
#include <boost/mysql/impl/internal/sansio/next_action.hpp>
#include <boost/mysql/impl/internal/sansio/sansio_algorithm.hpp>

#include <boost/asio/coroutine.hpp>

#include <string>

namespace boost {
namespace mysql {
namespace detail {

class set_character_set_algo : public sansio_algorithm, asio::coroutine
{
    diagnostics* diag_;
    character_set charset_;
    std::uint8_t seqnum_{0};

    static std::string compose_query(const character_set& charset)
    {
        BOOST_ASSERT(charset.name != nullptr);

        // Charset names must not have special characters.
        // TODO: this can be improved to use query formatting when we implement it.
        // See https://github.com/boostorg/mysql/issues/69
        string_view charset_name = charset.name;
        BOOST_ASSERT(charset_name.find('\'') == string_view::npos);

        std::string res("SET NAMES '");
        res.append(charset_name.data(), charset_name.size());
        res.push_back('\'');
        return res;
    }

public:
    set_character_set_algo(connection_state_data& st, set_character_set_algo_params params) noexcept
        : sansio_algorithm(st), diag_(params.diag), charset_(params.charset)
    {
    }

    next_action resume(error_code ec)
    {
        if (ec)
            return ec;

        // SET NAMES never returns rows. Using execute requires us to allocate
        // a results object, which we can avoid by simply sending the query and reading the OK response.
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Setup
            diag_->clear();

            // Send the execution request
            BOOST_ASIO_CORO_YIELD return write(query_command{compose_query(charset_)}, seqnum_);

            // Read the response
            BOOST_ASIO_CORO_YIELD return read(seqnum_);

            // Verify it's what we expected
            ec = deserialize_ok_response(st_->reader.message(), st_->flavor, *diag_, st_->backslash_escapes);
            if (ec)
                return ec;

            // If we were successful, update the character set
            st_->current_charset = charset_;
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
