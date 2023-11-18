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

// API

// GET /notes
struct get_notes_request
{
};
struct multi_notes_response
{
    std::vector<note_t> notes;
};
BOOST_DESCRIBE_STRUCT(multi_notes_response, (), (notes))

// POST /notes
struct create_note_body
{
    std::string title;
    std::string content;
};
BOOST_DESCRIBE_STRUCT(create_note_body, (), (title, content))
struct create_note_response
{
    std::int64_t id;
};
BOOST_DESCRIBE_STRUCT(create_note_response, (), (id))

// GET /notes/<note-id>
struct get_note_request
{
    std::int64_t id;
};
struct single_note_response
{
    note_t note;
};
BOOST_DESCRIBE_STRUCT(single_note_response, (), (note))

// PUT /notes/<note-id>
struct replace_note_request
{
    std::string title;
    std::string content;
};
struct replace_note_response
{
    note_t note;
};

// DELETE /notes/<note-id>
struct delete_note_request
{
    std::int64_t id;
};
struct delete_note_response
{
    bool deleted;
};
BOOST_DESCRIBE_STRUCT(delete_note_response, (), (deleted))

}  // namespace orders

#endif
