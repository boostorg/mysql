#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.8...3.22)

# Project
project(boost_mysql_mrdocs LANGUAGES CXX)

# MrDocs forces CMAKE_EXPORT_COMPILE_COMMANDS=ON, incorrectly
# causing all targets to be dumped to the compilation database.
# Disable this setting so we can set EXPORT_COMPILE_COMMANDS
# only to our target of interest
set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)

# Add Boost
add_subdirectory($ENV{BOOST_SRC_DIR} deps/boost)

# Add the target for mrdocs to analyze
add_executable(mrdocs mrdocs.cpp)
target_link_libraries(mrdocs PRIVATE Boost::mysql)
set_target_properties(mrdocs PROPERTIES EXPORT_COMPILE_COMMANDS ON)
