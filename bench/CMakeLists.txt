#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Utility target
add_custom_target(boost_mysql_bench)

# Adds a benchmark that uses Boost.MySQL
function (add_bench cpp_name)
    set(target_name "boost_mysql_bench_${cpp_name}")
    add_executable(${target_name} ${cpp_name}.cpp)
    target_link_libraries(${target_name} PUBLIC boost_mysql_compiled)
    boost_mysql_common_target_settings(${target_name})
    add_dependencies(boost_mysql_bench ${target_name})
endfunction()

# Adds a benchmark that uses the official C API
function (add_bench_libmysqlclient cpp_name)
    set(target_name "boost_mysql_bench_${cpp_name}")
    add_executable(${target_name} ${cpp_name}.cpp)
    target_link_libraries(${target_name} PRIVATE mysqlclient z zstd ssl crypto resolv m)
    add_dependencies(boost_mysql_bench ${target_name})
endfunction()


# Connection pool
add_bench(connection_pool)

# Protocol benchmarks
add_bench(one_row_boost)
add_bench_libmysqlclient(one_row_libmysqlclient)
add_bench(many_rows_boost)
add_bench_libmysqlclient(many_rows_libmysqlclient)
add_bench(stmt_params_boost)
add_bench_libmysqlclient(stmt_params_libmysqlclient)
