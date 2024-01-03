#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

add_library(
    boost_mysql_docs_lib
    STATIC
    EXCLUDE_FROM_ALL
    src.cpp
)
target_compile_definitions(
    boost_mysql_docs_lib
    PUBLIC
    BOOST_MYSQL_SEPARATE_COMPILATION
)
target_link_libraries(boost_mysql_docs_lib PUBLIC boost_mysql)
