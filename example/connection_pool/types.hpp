//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_ORDER_MANAGEMENT_HTTP_API_TYPES_HPP
#define BOOST_MYSQL_EXAMPLE_ORDER_MANAGEMENT_HTTP_API_TYPES_HPP

#include <boost/core/span.hpp>
#include <boost/describe/class.hpp>
#include <boost/optional/optional.hpp>

#include <string>
#include <vector>

namespace orders {

struct note_t
{
    // The unique database ID of the object.
    std::int64_t id;

    // The order status (draft, pending_payment, complete).
    std::string title;

    std::string content;
};
BOOST_DESCRIBE_STRUCT(note_t, (), (id, title, content))

//
// API requests.
//

// Used for creating and replacing notes
struct note_request_body
{
    std::string title;
    std::string content;
};
BOOST_DESCRIBE_STRUCT(note_request_body, (), (title, content))

//
// API responses
//

// Used for endpoints returning several notes
struct multi_notes_response
{
    std::vector<note_t> notes;
};
BOOST_DESCRIBE_STRUCT(multi_notes_response, (), (notes))

// Used for endpoints returning a single note
struct single_note_response
{
    note_t note;
};
BOOST_DESCRIBE_STRUCT(single_note_response, (), (note))

// Used for the delete note endpoint
struct delete_note_response
{
    bool deleted;
};
BOOST_DESCRIBE_STRUCT(delete_note_response, (), (deleted))

}  // namespace orders

#endif
