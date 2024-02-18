//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/static_results.hpp>

#ifdef BOOST_MYSQL_CXX14

//[example_connection_pool_repository_cpp
//
// File: repository.cpp
//

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/throw_on_error.hpp>

#include <iterator>
#include <tuple>
#include <utility>

#include "repository.hpp"
#include "types.hpp"

using namespace notes;
namespace mysql = boost::mysql;

// SQL code to create the notes table is located under $REPO_ROOT/example/db_setup.sql
// The table looks like this:
//
// CREATE TABLE notes(
//     id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
//     title TEXT NOT NULL,
//     content TEXT NOT NULL
// );

std::vector<note_t> note_repository::get_notes(boost::asio::yield_context yield)
{
    mysql::diagnostics diag;
    mysql::error_code ec;

    // Get a fresh connection from the pool. This returns a pooled_connection object,
    // which is a proxy to an any_connection object. Connections are returned to the
    // pool when the proxy object is destroyed.
    mysql::pooled_connection conn = pool_.async_get_connection(diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // Execute the query to retrieve all notes. We use the static interface to
    // parse results directly into static_results.
    mysql::static_results<note_t> result;
    conn->async_execute("SELECT id, title, content FROM notes", result, diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // By default, connections are reset after they are returned to the pool
    // (by using any_connection::async_reset_connection). This will reset any
    // session state we changed while we were using the connection
    // (e.g. it will deallocate any statements we prepared).
    // We did nothing to mutate session state, so we can tell the pool to skip
    // this step, providing a minor performance gain.
    // We use pooled_connection::return_without_reset to do this.
    conn.return_without_reset();

    // Move note_t objects into the result vector to save allocations
    return std::vector<note_t>(
        std::make_move_iterator(result.rows().begin()),
        std::make_move_iterator(result.rows().end())
    );
}

optional<note_t> note_repository::get_note(std::int64_t note_id, boost::asio::yield_context yield)
{
    mysql::diagnostics diag;
    mysql::error_code ec;

    // Get a fresh connection from the pool. This returns a pooled_connection object,
    // which is a proxy to an any_connection object. Connections are returned to the
    // pool when the proxy object is destroyed.
    mysql::pooled_connection conn = pool_.async_get_connection(diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // Our query has a parameter, so we need a prepared statement.
    // We don't need to deallocate this statement explicitly
    // (no need to call any_connection::async_close_statement).
    // The connection pool takes care of this for us
    // (by using any_connection::async_reset_connection).
    mysql::statement stmt = conn->async_prepare_statement(
        "SELECT id, title, content FROM notes WHERE id = ?",
        diag,
        yield[ec]
    );
    mysql::throw_on_error(ec, diag);

    // Execute the statement. We use the static interface to parse results
    mysql::static_results<note_t> result;
    conn->async_execute(stmt.bind(note_id), result, diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // An empty results object indicates that no note was found
    if (result.rows().empty())
        return {};
    else
        return std::move(result.rows()[0]);

    // There's no need to return the connection explicitly to the pool,
    // pooled_connection's destructor takes care of it.
}

note_t note_repository::create_note(string_view title, string_view content, boost::asio::yield_context yield)
{
    mysql::diagnostics diag;
    mysql::error_code ec;

    // Get a fresh connection from the pool. This returns a pooled_connection object,
    // which is a proxy to an any_connection object. Connections are returned to the
    // pool when the proxy object is destroyed.
    mysql::pooled_connection conn = pool_.async_get_connection(diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // Our query has parameters, so we need to prepare a statement.
    // As explained above, there is no need to deallocate the statement explicitly,
    // since the pool takes care of it.
    mysql::statement stmt = conn->async_prepare_statement(
        "INSERT INTO notes (title, content) VALUES (?, ?)",
        diag,
        yield[ec]
    );
    mysql::throw_on_error(ec, diag);

    // Execute the statement. The statement won't produce any rows,
    // so we can use static_results<std::tuple<>>
    mysql::static_results<std::tuple<>> result;
    conn->async_execute(stmt.bind(title, content), result, diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // MySQL reports last_insert_id as a uint64_t regardless of the actual ID type.
    // Given our table definition, this cast is safe
    auto new_id = static_cast<std::int64_t>(result.last_insert_id());

    return note_t{new_id, title, content};

    // There's no need to return the connection explicitly to the pool,
    // pooled_connection's destructor takes care of it.
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

    // Get a fresh connection from the pool. This returns a pooled_connection object,
    // which is a proxy to an any_connection object. Connections are returned to the
    // pool when the proxy object is destroyed.
    mysql::pooled_connection conn = pool_.async_get_connection(diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // Our query has parameters, so we need to prepare a statement.
    // As explained above, there is no need to deallocate the statement explicitly,
    // since the pool takes care of it.
    mysql::statement stmt = conn->async_prepare_statement(
        "UPDATE notes SET title = ?, content = ? WHERE id = ?",
        diag,
        yield[ec]
    );
    mysql::throw_on_error(ec, diag);

    // Execute the statement. The statement won't produce any rows,
    // so we can use static_results<std::tuple<>>
    mysql::static_results<std::tuple<>> empty_result;
    conn->async_execute(stmt.bind(title, content, note_id), empty_result, diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // No affected rows means that the note doesn't exist
    if (empty_result.affected_rows() == 0u)
        return {};

    return note_t{note_id, title, content};

    // There's no need to return the connection explicitly to the pool,
    // pooled_connection's destructor takes care of it.
}

bool note_repository::delete_note(std::int64_t note_id, boost::asio::yield_context yield)
{
    mysql::diagnostics diag;
    mysql::error_code ec;

    // Get a fresh connection from the pool. This returns a pooled_connection object,
    // which is a proxy to an any_connection object. Connections are returned to the
    // pool when the proxy object is destroyed.
    mysql::pooled_connection conn = pool_.async_get_connection(diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // Our query has parameters, so we need to prepare a statement.
    // As explained above, there is no need to deallocate the statement explicitly,
    // since the pool takes care of it.
    mysql::statement stmt = conn->async_prepare_statement("DELETE FROM notes WHERE id = ?", diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // Execute the statement. The statement won't produce any rows,
    // so we can use static_results<std::tuple<>>
    mysql::static_results<std::tuple<>> empty_result;
    conn->async_execute(stmt.bind(note_id), empty_result, diag, yield[ec]);
    mysql::throw_on_error(ec, diag);

    // No affected rows means that the note didn't exist
    return empty_result.affected_rows() != 0u;

    // There's no need to return the connection explicitly to the pool,
    // pooled_connection's destructor takes care of it.
}

//]

#endif
