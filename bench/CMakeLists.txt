#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Utility target
add_custom_target(boost_mysql_bench)

# Find libraries required by the official clients
find_package(OpenSSL REQUIRED)

# Find libmysqlclient. This script has only been tested on Ubuntu Linux, where benchmarks are performed.
find_path(LIBMYSQLCLIENT_INCLUDE_DIR mysql/mysql.h)
if (NOT LIBMYSQLCLIENT_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find libmysqlclient includes")
endif()
find_library(LIBMYSQLCLIENT_LIBRARY mysqlclient)
if (NOT LIBMYSQLCLIENT_LIBRARY)
    message(FATAL_ERROR "Could not find the libmysqlclient library binary")
endif()
add_library(boost_mysql_libmysqlclient SHARED IMPORTED)
set_property(TARGET boost_mysql_libmysqlclient PROPERTY IMPORTED_LOCATION ${LIBMYSQLCLIENT_LIBRARY})
target_include_directories(boost_mysql_libmysqlclient INTERFACE ${LIBMYSQLCLIENT_INCLUDE_DIR})
target_link_libraries(boost_mysql_libmysqlclient INTERFACE OpenSSL::SSL OpenSSL::Crypto)

# Find libmariadb
find_path(LIBMARIADB_INCLUDE_DIR mariadb/mysql.h)
if (NOT LIBMARIADB_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find libmariadb includes")
endif()
find_library(LIBMARIADB_LIBRARY mariadb)
if (NOT LIBMARIADB_LIBRARY)
    message(FATAL_ERROR "Could not find the libmariadb library binary")
endif()
add_library(boost_mysql_libmariadb SHARED IMPORTED)
set_property(TARGET boost_mysql_libmariadb PROPERTY IMPORTED_LOCATION ${LIBMARIADB_LIBRARY})
target_include_directories(boost_mysql_libmariadb INTERFACE ${LIBMARIADB_INCLUDE_DIR})
target_link_libraries(boost_mysql_libmariadb INTERFACE OpenSSL::SSL OpenSSL::Crypto)

# Adds a benchmark target - pass the libraries to link to as extra args
function (add_bench cpp_name)
    set(target_name "boost_mysql_bench_${cpp_name}")
    add_executable(${target_name} ${cpp_name}.cpp)
    target_link_libraries(${target_name} PUBLIC ${ARGN})
    boost_mysql_common_target_settings(${target_name})
    add_dependencies(boost_mysql_bench ${target_name})
endfunction()

# We use the header-only version here because it's slightly faster
# and to avoid dependencies to the test CMake scripts

# Connection pool
add_bench(connection_pool boost_mysql)

# Protocol benchmarks
add_bench(one_small_row_boost boost_mysql)
add_bench(one_small_row_libmysqlclient boost_mysql_libmysqlclient)
add_bench(one_small_row_libmariadb boost_mysql_libmariadb)

add_bench(one_big_row_boost boost_mysql)
add_bench(one_big_row_libmysqlclient boost_mysql_libmysqlclient)
add_bench(one_big_row_libmariadb boost_mysql_libmariadb)

add_bench(many_rows_boost boost_mysql)
add_bench(many_rows_libmysqlclient boost_mysql_libmysqlclient)
add_bench(many_rows_libmariadb boost_mysql_libmariadb)

add_bench(stmt_params_boost boost_mysql)
add_bench(stmt_params_libmysqlclient boost_mysql_libmysqlclient)
add_bench(stmt_params_libmariadb boost_mysql_libmariadb)
