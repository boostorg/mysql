# \<Proposed for\> Boost.MySQL (not yet in Boost)

 Linux/OSX | Windows | Coverage | Documentation
-----------|---------|----------|--------------
[![Build Status](https://github.com/anarthal/mysql/actions/workflows/build-code/badge.svg)](https://github.com/anarthal/mysql) | [![Build status](https://ci.appveyor.com/api/projects/status/slqnb8mt91v33p1y/branch/master?svg=true)](https://ci.appveyor.com/project/anarthal/mysql/branch/master) | [![codecov](https://codecov.io/gh/anarthal/mysql/branch/master/graph/badge.svg)](https://codecov.io/gh/anarthal/mysql-asio/branch/master) | [![Build docs](https://github.com/anarthal/mysql/actions/workflows/build-docs.yml/badge.svg)](https://github.com/anarthal/mysql/actions/workflows/build-docs.yml)

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

- C++11 capable compiler (tested with gcc 5 to 11, clang 3.6 to 13 and MSVC 19.25).
- Boost.
- OpenSSL.
- CMake 3.13.0 or higher, if using CMake to build against the library (this is the preferred way).
- Tested with MySQL v5.7.29, MySQL v8.0.19, MariaDB v10.3 and MariaDB v10.5.

## Versioning and upgrading

The current latest version is 0.1.0. Until Boost.Mysql passes its Boost formal review,
the library might get non-backwards-compatible changes between minor versions.
Sorry for that! Any breaking change will be listed here, together with a rationale and
an upgrade guide. If you encounter any trouble, please open an issue in the repository.

### Breaking changes from 0.0.x to 0.1.x

This version has changed the way SSL is handled by the library to
a more standard approach, similar to what Boost.Beast and Boost.Asio use.
It has also introduced some naming changes. The full list of breaking changes follows:

* The `connection` object no longer uses SSL/TLS implicitly. To enable SSL/TLS,
  you must explicitly use a `Stream` type supporting SSL/TLS.
  * **Rationale**: in 0.0.x, `connection` objects created a hidden SSL stream on top of the provided stream.
    This approach is confusing and non-standard, and allowed little control over SSL options to the user.
    You were probably using SSL without knowing you were.
  * **How to upgrade**:
    * If you were using `tcp_connection` or `unix_connection` and you want
      to keep using SSL, use the new `tcp_ssl_connection` or `unix_ssl_connection`, instead.
      You will need to create a `boost::asio::ssl::context` and pass it to the connection constructor.
      All of the examples use SSL, so you can review those.
    * If you were using `tcp_connection` or `unix_connection` and you don't want to
      use SSL, you don't need to change anything.
    * If you were directly using the template classes `connection<Stream>` or `socket_connection<Stream>`,
      and you want to keep using SSL, you will need to use `connection<boost::asio::ssl::stream<Stream>>`, instead.
      You will need to create a `boost::asio::ssl::context` and pass it to the connection constructor.
    * If you were directly using the template classes `connection<Stream>` or `socket_connection<Stream>`,
      and you don't want to use SSL, you don't need to change anything.
* The `ssl_options` struct has been removed, in favor of its only member, `ssl_mode`.
  * **Rationale**: `ssl_options` would eventually include options to be set on the `ssl::context` that was 
    implicitly created by `connection`. You can now provide a `ssl::context` with all the options, so this is no longer required.
  * **How to upgrade**:
    * If you were using the struct `ssl_options(ssl_mode::xxxx)` anywhere, replace it by `ssl_mode::xxxx`.
    * If you were using `connection_params::ssl()`, this function now returns an instance of `ssl_mode`, instead of `ssl_options`.
    * Otherwise, you don't need to do anything.
* The `socket_connection` class has been removed, and its functionality has been merged into `connection`.
  * **Rationale**: `socket_connection` added complexity with no added value.
  * **How to upgrade**: if you were explicitly using `socket_connection<MyStreamType>`, replace it by `connection<MyStreamType>`.
    If you were using the `tcp_connection` and `unix_connection` type aliases, you don't need to do anything.
* Type aliases for `connection`, `prepared_statement` and `resultset` have moved to separate files.
  * **Rationale**: as more type aliases become available, the `connection.hpp` header would include more and more unwanted dependencies.
  * **How to upgrade**: use the new header files `tcp.hpp`, `tcp_ssl.hpp`, `unix.hpp` and `unix_ssl.hpp`.
* The convenience header `boost/mysql/mysql.hpp` has been moved to `boost/mysql.hpp`.
  * **Rationale**: the first path was a mistake, in the first place. All other Boost libs follow the other convention.
  * **How to upgrade**: if you were using the `boost/mysql/mysql.hpp` header, replace it by `boost/mysql.hpp`.
    Otherwise, you don't need to do anything.
* The `prepared_statement::execute` iterator overload has been replaced by an overload taking an `execute_params` struct,
  encapsulating those iterators.
  * **Rationale**: this approach is more extensible. When adding further optional parameters to `execute`, we don't need
    to create extra overloads.
  * **How to upgrade**: replace `prepared_statement::execute(it1, it2)` calls by `prepared_statement::execute(make_execute_params(it1, it2))`.
* The `resultset::fetch_one`, `resultset::fetch_many`, `resultset::fetch_all` methods have been renamed to
  `read_one`, `read_many` and `read_all`.
  * **Rationale**: these functions read rows that the MySQL server has already sent. The term _fetch_ is used in the MySQL protocol
    to ask the server for extra rows under certain circumstances. We would like to be able to implement this protocol feature in the future
    without causing massive confusion.
  * **How to upgrade**: apply the renaming.
* There are no longer `row`s and `owning_row`s. Instead, all rows now own the memory for their values and are simply called `row`s.
  The `resultset::read_one` (old `resultset::fetch_one`) function now takes a `row` lvalue reference and returns a boolean indicating whether a row was read or not.
  * **Rationale**: the old mechanics for `resultset::fetch_one` didn't allow for efficient composition, as the returned row was owned by the
    `resultset` and had to be copied before further reading. With the new approach, `read_many` and `read_all` can be implemented in terms of `fetch_one`.
  * **How to upgrade**: replace loops like `while (const row* r = resultset.fetch_one()) {}` by `row r; while (resultset.fetch_one(r))` (and their async equivalents).
* `NULL` values are now represented as a custom type `boost::mysql::null_t` (which is an alias for `boost::variant2::monostate`), instead of `std::nullptr_t`.
  * **Rationale**: relational operators like `<` don't work for `std::nullptr_t` values, which means you can't have a `std::set<value>`.
    `boost::variant2::monostate` is the right tool to represent values like `NULL`.
  * **How to upgrade**:
     * If you were using `boost::variant2::visit` on the underlying variant returned by `value::to_variant`, you need to add an overload
       taking a `null_t` to the visitor function.
     * If you were using `value::is<std::nullptr_t>`, replace it by `value::is_null` or `value::is<null_t>`.
* `value::get` no longer throws `boost::variant2::bad_variant_access`, but a custom exception type `boost::mysql::bad_value_access`.
  * **Rationale**: it provides finer control over exception handling.
  * **How to upgrade**: if you were catching `boost::variant2::bad_variant_access` as a result of a `value::get` call, replace the exception
    type in the `catch` clause by `boost::mysql::bad_value_access`.
    

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
