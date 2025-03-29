#!/usr/bin/python3
#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Functions to run the benchmarks. The actual bench build is in cmake.py

from pathlib import Path
from subprocess import check_output

def run_connection_pool_bench(
    exe_dir: Path,
    iters: int,
    server_host: str,
) -> None:
    # The benchmarks to run
    benchmarks = [
        "nopool-tcp",
        "nopool-tcpssl",
        "nopool-unix",
        "pool-tcp",
        "pool-tcpssl",
        "pool-unix",
    ]

    exe = exe_dir.joinpath('boost_mysql_bench_connection_pool')

    for bench in benchmarks:
        for _ in range(iters):
            time = int(check_output([exe, bench, server_host]).decode())
            print('{},{}'.format(bench, time))


def run_protocol_bench(
    exe_dir: Path,
    iters: int,
) -> None:
    # The benchmarks to run
    benchmarks = [
        "one_row_boost",
        "one_row_libmysqlclient",
        "one_row_libmariadb",
        "many_rows_boost",
        "many_rows_libmysqlclient",
        "many_rows_libmariadb",
        "stmt_params_boost",
        "stmt_params_libmysqlclient",
        "stmt_params_libmariadb",
    ]

    for bench in benchmarks:
        exe = exe_dir.joinpath('boost_mysql_bench_' + bench)
        for _ in range(iters):
            time = int(check_output([exe]).decode())
            print('{},{}'.format(bench, time))

