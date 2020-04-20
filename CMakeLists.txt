#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.13.0) # Determined by requiring Boost 1.70
project(mysql-asio)

# We require having access to cmake/ in the module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# If we are the top-level project, enable testing
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        set(_MYSQL_TESTING_ENABLED ON)
    endif()
endif()

# Valgrind tests and Valgrind-friendly code (e.g. mark SSL buffers as initialized)
option(BOOST_MYSQL_VALGRIND_TESTS OFF "Whether to run Valgrind tests or not (requires Valgrind)")
mark_as_advanced(BOOST_MYSQL_VALGRIND_TESTS)

# Some common utilities
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/utils.cmake)

# Dependencies
include(FetchContent)

find_package(Boost 1.70.0 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

if (BOOST_MYSQL_VALGRIND_TESTS)
    find_package(Mysqlvalgrind REQUIRED)
endif()

# Date
FetchContent_Declare(
    date
    GIT_REPOSITORY https://github.com/HowardHinnant/date.git
    GIT_TAG        v2.4.1
)

FetchContent_GetProperties(date)
if(NOT date_POPULATED)
    FetchContent_Populate(date)
    # Building date requires CMake 3.14, and we only need headers
endif()

# Interface library (header-only)
add_library(
    Boost_mysql
    INTERFACE
)

target_link_libraries(
    Boost_mysql
    INTERFACE
    Boost::system
    Threads::Threads
    OpenSSL::Crypto
    OpenSSL::SSL
)
target_include_directories(
    Boost_mysql
    INTERFACE
    include
    ${date_SOURCE_DIR}/include
)
target_compile_features(
    Boost_mysql
    INTERFACE
    cxx_std_17
)

if (BOOST_MYSQL_VALGRIND_TESTS)
    target_link_libraries(
        Boost_mysql
        INTERFACE
        Mysqlvalgrind::Mysqlvalgrind
    )
endif()

# Examples and tests
if(_MYSQL_TESTING_ENABLED)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_utils.cmake)
    add_subdirectory(example)
    add_subdirectory(test)
endif()
