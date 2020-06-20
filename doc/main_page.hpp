//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

/**
 * \mainpage User manual
 *
 * Boost.MySQL is a C++11 client for the MySQL database server, based on Boost.Asio.
 *
 * \section why Why another MySQL C++ client?
 *
 * - It is fully compatible with Boost.Asio and integrates well with any other
 *   library in the Boost.Asio ecosystem (like Boost.Beast).
 * - It supports Boost.Asio's universal asynchronous model, which means you can
 *   go asyncrhonous using callbacks, futures or coroutines (including C++20 coroutines).
 * - It is written in modern C++ (C++11) and takes advantage of the latest language
 *   features and standard library additions.
 * - It is header only.
 *
 * \section building Building
 *
 * As this is a header-only library, you do not need to build it. However, as it
 * has a bunch of dependencies, we suggest you use CMake to pull them in as you build
 * your application.
 *
 * Download Boost.MySQL and make it available to your CMake script (we suggest you use
 * CMake's FetchContent module to do this), and then call add_subdirectory() on the
 * Boost.MySQL root directory. This will look for all the required dependencies.
 *
 * Finally, link your target against the **Boost::mysql** interface library, and you will be done!
 *
 * \section Requirements
 *
 * - C++11 capable compiler (tested with gcc 7.4, clang 7.0, Apple clang 11.0, MSVC 19.25).
 * - Boost 1.72 or higher.
 * - OpenSSL.
 * - CMake 3.13.0 or higher, if using CMake to build against the library (this is the preferred way).
 * - Tested with MySQL v5.7.29, MySQL v8.0.19, MariaDB v10.3 and MariaDB v10.5.
 *
 * \section Features
 * - Text queries (execution of text SQL queries and data retrieval).
 *   MySQL refers to this as the "text protocol", as all information is passed using text
 *   (as opposed to prepared statements, see below).
 * - Prepared statements. MySQL refers to this as the "binary protocol", as the result
 *   of executing a prepared statement is sent in binary format rather than in text.
 * - Authentication methods (authentication plugins): mysql_native_password and
 *   caching_sha2_password. These are the default methods in MySQL 5 and MySQL 8,
 *   respectively.
 * - Encrypted connections (TLS).
 *
 * \section tutorial Tutorial
 * This tutorial shows an example of how to use the Boost.MySQL library.
 * It employs synchronous functions with exceptions as error handling strategy,
 * which is the simplest.
 *
 * \subsection tutorial_code Tutorial code
 * \include query_sync.cpp
 *
 * \subsection tutorial_db_setup Database setup
 * You can run the `db_setup.sql` file included with the
 * example to set it up, or copy and paste the following commands:
 * \include db_setup.sql
 *
 * \section examples_subsection Examples
 *
 * \subsection query_async_callbacks Query, asynchronous with callbacks
 * This example demonstrates issuing text queries to the server and reading
 * results, using callbacks.
 * \include query_async_callbacks.cpp
 *
 * \subsection query_async_futures Query, asynchronous with futures
 * This example demonstrates issuing text queries to the server and reading
 * results, using futures.
 * \include query_async_futures.cpp
 *
 * \subsection query_async_coroutines Query, asynchronous with coroutines
 * This example demonstrates issuing text queries to the server and reading
 * results, using coroutines.
 * \include query_async_coroutines.cpp
 *
 * \subsection query_async_coroutinescpp20 Query, asynchronous with C++20 coroutines
 * This example demonstrates issuing text queries to the server and reading
 * results, using C++20 coroutines (boost::asio::use_awaitable).
 * \include query_async_coroutinescpp20.cpp
 *
 * \subsection prepared_statements Prepared statements
 * This example demonstrates preparing statements, executing them
 * and reading back the results. It employs synchronous functions.
 * \include prepared_statements.cpp
 *
 * \subsection metadata Metadata
 * This example demonstrates inspecting the metadata of a resultset.
 * \include metadata.cpp
 *
 * \subsection unix_socket UNIX domain sockets
 * This example demonstrates connecting to a MySQL server over
 * a UNIX domain socket. It employs synchronous functions with
 * exceptions.
 * \include unix_socket.cpp
 */



