//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "repository.hpp"

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/pooled_connection.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <boost/core/span.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <string>
#include <tuple>
#include <utility>

#include "types.hpp"

using namespace orders;
namespace mysql = boost::mysql;

std::vector<note_t> note_repository::get_notes(boost::asio::yield_context yield)
{
    mysql::diagnostics diag;
    mysql::error_code ec;

    mysql::pooled_connection conn = pool_.async_get_connection(diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    mysql::static_results<note_t> result;
    conn->async_execute("SELECT id, title, content FROM notes", result, diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    conn.return_without_reset();

    return std::vector<note_t>(
        std::make_move_iterator(result.rows().begin()),
        std::make_move_iterator(result.rows().end())
    );
}

optional<note_t> note_repository::get_note(std::int64_t note_id, boost::asio::yield_context yield)
{
    mysql::diagnostics diag;
    mysql::error_code ec;

    mysql::pooled_connection conn = pool_.async_get_connection(diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    mysql::statement stmt = conn->async_prepare_statement(
        "SELECT id, title, content FROM notes WHERE id = ?",
        diag,
        yield[ec]
    );
    mysql::throw_on_error(ec, diag);

    mysql::static_results<note_t> result;
    conn->async_execute(stmt.bind(note_id), result, diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    if (result.rows().empty())
        return {};
    else
        return std::move(result.rows()[0]);
}

note_t note_repository::create_note(string_view title, string_view content, boost::asio::yield_context yield)
{
    mysql::diagnostics diag;
    mysql::error_code ec;

    mysql::pooled_connection conn = pool_.async_get_connection(diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    mysql::statement stmt = conn->async_prepare_statement(
        "INSERT INTO notes (title, content) VALUES (?, ?)",
        diag,
        yield[ec]
    );
    mysql::throw_on_error(ec, diag);

    mysql::static_results<std::tuple<>> result;
    conn->async_execute(stmt.bind(title, content), result, diag, yield[ec]);
    mysql::throw_on_error(ec, diag);
    auto new_id = static_cast<std::int64_t>(result.last_insert_id());

    return note_t{new_id, title, content};
}

optional<note_t> note_repository::replace_note(
    std::int64_t note_id,
    string_view title,
    string_view content,
    boost::asio::yield_context yield
)
{
    mysql::diagnostics diag;
    mysql::error_code ec;

    // Get a connection
    mysql::pooled_connection conn = pool_.async_get_connection(diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // Prepare the statement
    mysql::statement stmt = conn->async_prepare_statement(
        "UPDATE notes SET title = ?, content = ? WHERE id = ?",
        diag,
        yield[ec]
    );
    mysql::throw_on_error(ec, diag);

    // Update the note
    mysql::static_results<std::tuple<>> empty_result;
    conn->async_execute(stmt.bind(title, content, note_id), empty_result, diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    if (empty_result.affected_rows() == 0u)
        return {};

    return note_t{note_id, title, content};
}

bool note_repository::delete_note(std::int64_t note_id, boost::asio::yield_context yield)
{
    mysql::diagnostics diag;
    mysql::error_code ec;

    // Get a connection
    mysql::pooled_connection conn = pool_.async_get_connection(diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    mysql::statement stmt = conn->async_prepare_statement("DELETE FROM notes WHERE id = ?", diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    mysql::static_results<std::tuple<>> empty_result;
    conn->async_execute(stmt.bind(note_id), empty_result, diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    return empty_result.affected_rows() > 0u;
}