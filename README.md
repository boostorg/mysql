# MySQL-Asio

TravisCI: [![Build Status](https://travis-ci.com/anarthal/mysql-asio.png?branch=master)](https://github.com/anarthal/mysql-asio)
AppVeyor: [![Build status](https://ci.appveyor.com/api/projects/status/slqnb8mt91v33p1y/branch/master?svg=true)](https://ci.appveyor.com/project/anarthal/mysql-asio/branch/master)

MySQL-Asio is a C++17 client for the MySQL database server, based on Boost.Asio.

## Why another MySQL C++ client?

- It is fully compatible with Boost.Asio and integrates well with any other
  library in the Boost.Asio ecosystem (like Boost.Beast).
- It supports Boost.Asio's universal asynchronous model, which means you can
  go asyncrhonous using callbacks, futures or coroutines.
- It is written in modern C++ (C++17) and takes advantage of the latest language
  features and standard library additions.
- It is header only.

## Building

As this is a header-only library, you do not need to build it. However, as it
has a bunch of dependencies, we suggest you use CMake to pull them in as you build
your application.

Download MySQL-Asio and make it available to your CMake script (we suggest you use
CMake's FetchContent module to do this), and then call add_subdirectory() on the
MySQL-Asio root directory. This will look for all the required dependencies.

Finally, link your target against the **mysql_asio** interface library, and you will be done!

## Requirements

- C++17 capable compiler (tested with gcc 7.4, clang 7.0, Apple clang 11.0, MSVC 19.25).
- Boost 1.70 or higher. The following Boost libraries are used:
    - Boost.Asio (and in consequence, Boost.System).
    - Boost.Beast (implementation dependency, we are working in removing it).
    - Boost.Lexical_Cast.
    - Boost.Endian.
- OpenSSL.
- CMake 3.11.0 or higher, if using CMake to build against the library (this is the preferred way).
- Howard Hinnant's date library (https://github.com/HowardHinnant/date) v2.4.1 or higher.
  If you are using CMake to link against MySQL-Asio (the preferred way), it will be fetched automatically.
  (no need for a manual download).
- Tested with MySQL v5.7.29, MySQL v8.0.19, MariaDB v10.3 and MariaDB v10.5.

## Documentation and examples

There are some examples under the examples/ folder in this repository. Please start
by looking into query_sync.cpp, as this one demonstrates the basic concepts in the library.

There is no online documentation (yet!). Please check the Doxygen-style comments in the source
code.

Header files within include/mysql are public. Header files within include/mysql/impl are
private and their content may (and will) change over time with no backwards-compatibility guarantee.

## Features

Currently implemented:
- Text queries (execution of text SQL queries and data retrieval).
  MySQL refers to this as the "text protocol", as all information is passed using text
  (as opposed to prepared statements, see below).
- Prepared statements. MySQL refers to this as the "binary protocol", as the result
  of executing a prepared statement is sent in binary format rather than in text.
- User/password basic authentication (mysql_native_password). MySQL supports several
  authentication methods ("authentication plugins"). Only the mysql_native_password
  is supported yet.

Yet to be done (but it is on our list - PRs welcome):

- Further authentication methods: caching_sha2_password, sha256_password...
- SSL encrypted connections.
