#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

add_executable(
    boost_mysql_snippets
    snippets.cpp
)
target_include_directories(
    boost_mysql_snippets
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/include"
)
target_link_libraries(
    boost_mysql_snippets
    PRIVATE
    boost_mysql_compiled
)
boost_mysql_common_target_settings(boost_mysql_snippets)

add_test(
    NAME boost_mysql_snippets
    COMMAND boost_mysql_snippets
)
