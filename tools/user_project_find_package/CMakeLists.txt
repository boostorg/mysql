#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# A sample project that imports Boost.Mysql using find_package
# Used to test our CMake exports

cmake_minimum_required(VERSION 3.13.0)
project(user_project)

find_package(boost_mysql REQUIRED)

add_executable(main query_sync.cpp)
target_link_libraries(main PRIVATE Boost::mysql)

enable_testing()
add_test(
    NAME test 
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/main example_user example_password
)
