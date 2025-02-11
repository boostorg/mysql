//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_HANDLE_REQUEST_HPP
#define BOOST_MYSQL_EXAMPLE_3_ADVANCED_HTTP_SERVER_CPP20_HANDLE_REQUEST_HPP

//[example_http_server_cpp20_handle_request_hpp
//
// File: handle_request.hpp
//

#include <boost/mysql/connection_pool.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

namespace orders {

// Handles an individual HTTP request, producing a response.
// The caller of this function should use response::version,
// response::keep_alive and response::prepare_payload to adjust the response.
boost::asio::awaitable<boost::beast::http::response<boost::beast::http::string_body>> handle_request(
    const boost::beast::http::request<boost::beast::http::string_body>& request,
    boost::mysql::connection_pool& pool
);

}  // namespace orders

//]

#endif
