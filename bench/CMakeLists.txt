#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#


add_executable(
    boost_mysql_bench_connection_pool
    connection_pool.cpp
)

target_link_libraries(
    boost_mysql_bench_connection_pool
    PUBLIC
    boost_mysql_compiled
)

boost_mysql_common_target_settings(boost_mysql_bench_connection_pool)

add_executable(
    main
    one_row_libmysqlclient.cpp
)
target_link_libraries(main PRIVATE mysqlclient z zstd ssl crypto resolv m)

add_executable(
    mainboost
    one_row_boost.cpp
)
target_link_libraries(
    mainboost
    PUBLIC
    boost_mysql_compiled
)
boost_mysql_common_target_settings(mainboost)




add_executable(
    many_rows_libmysqlclient
    many_rows_libmysqlclient.cpp
)
target_link_libraries(many_rows_libmysqlclient PRIVATE mysqlclient z zstd ssl crypto resolv m)

add_executable(
    many_rows_boost
    many_rows_boost.cpp
)
target_link_libraries(
    many_rows_boost
    PUBLIC
    boost_mysql_compiled
)
boost_mysql_common_target_settings(many_rows_boost)