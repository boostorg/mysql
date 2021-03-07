# Boost.MySQL

 Linux/OSX | Windows | Coverage
-----------|---------|----------
[![Build Status](https://travis-ci.com/anarthal/mysql.png?branch=master)](https://github.com/anarthal/mysql) | [![Build status](https://ci.appveyor.com/api/projects/status/slqnb8mt91v33p1y/branch/master?svg=true)](https://ci.appveyor.com/project/anarthal/mysql/branch/master) | [![codecov](https://codecov.io/gh/anarthal/mysql/branch/master/graph/badge.svg)](https://codecov.io/gh/anarthal/mysql-asio/branch/master)

Boost.Mysql is a C++11 client for the MySQL database server, based on Boost.Asio.
This library is in the process of being proposed for Boost.

Documentation and examples are [here](https://anarthal.github.io/mysql/index.html).

## Why another MySQL C++ client?

- It is fully compatible with Boost.Asio and integrates well with any other
  library in the Boost.Asio ecosystem (like Boost.Beast).
- It supports Boost.Asio's universal asynchronous model, which means you can
  go asyncrhonous using callbacks, futures or coroutines (including C++20 coroutines).
- It is written in modern C++ (C++11) and takes advantage of the latest language
  features and standard library additions.
- It is header only.

## Building

As this is a header-only library, you do not need to build it. However, as it
has a bunch of dependencies, we suggest you use CMake to pull them in as you build
your application.

Download Boost.MySQL and make it available to your CMake script (we suggest you use
CMake's FetchContent module to do this), and then call add_subdirectory() on the
Boost.MySQL root directory. This will look for all the required dependencies.

Finally, link your target against the **Boost::mysql** interface library, and you will be done!

## Requirements

- C++11 capable compiler (tested with gcc 7.4, clang 7.0, Apple clang 11.0, MSVC 19.25).
- Boost 1.72 or higher.
- OpenSSL.
- CMake 3.13.0 or higher, if using CMake to build against the library (this is the preferred way).
- Tested with MySQL v5.7.29, MySQL v8.0.19, MariaDB v10.3 and MariaDB v10.5.

## Features

Currently implemented:
- Text queries (execution of text SQL queries and data retrieval).
  MySQL refers to this as the "text protocol", as all information is passed using text
  (as opposed to prepared statements, see below).
- Prepared statements. MySQL refers to this as the "binary protocol", as the result
  of executing a prepared statement is sent in binary format rather than in text.
- Authentication methods (authentication plugins): mysql_native_password and
  caching_sha2_password. These are the default methods in MySQL 5 and MySQL 8,
  respectively.
- Encrypted connections (TLS).
- TCP and UNIX socket connections. The implementation is based on Boost.Asio
  SyncStream and AsyncStream concepts, so it is generic and can be used with
  any stream that fulfills these concept's requirements. There are user-friendly
  typedefs and regression tests for TCP and UNIX socket streams.

Yet to be done (but it is on our list - PRs welcome):

- Further authentication methods: sha256_password
- Multi-resultset: being able to specify several semicolon-separated queries. 


