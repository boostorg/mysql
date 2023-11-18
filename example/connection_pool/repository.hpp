//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_ORDER_MANAGEMENT_HTTP_BUSINESS_HPP
#define BOOST_MYSQL_EXAMPLE_ORDER_MANAGEMENT_HTTP_BUSINESS_HPP

#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/asio/spawn.hpp>
#include <boost/optional/optional.hpp>

#include <cstdint>

#include "types.hpp"

namespace orders {

using boost::optional;
using boost::mysql::string_view;

class note_repository
{
    boost::mysql::connection_pool& pool_;

public:
    note_repository(boost::mysql::connection_pool& pool) : pool_(pool) {}

    std::vector<note_t> get_notes(boost::asio::yield_context yield);
    optional<note_t> get_note(std::int64_t note_id, boost::asio::yield_context yield);
    note_t create_note(string_view title, string_view content, boost::asio::yield_context yield);
    optional<note_t> replace_note(
        std::int64_t note_id,
        string_view title,
        string_view content,
        boost::asio::yield_context yield
    );
    bool delete_note(std::int64_t note_id, boost::asio::yield_context yield);
};

}  // namespace orders

#endif
