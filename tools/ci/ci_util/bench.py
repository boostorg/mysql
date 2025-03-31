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

_results_file = 'benchmark-results.txt'


def _record_measurement(bench: str, time: int) -> None:
    print('{}: {}ms'.format(bench, time), flush=True)
    with open(_results_file, 'at') as f:
        f.write('{},{}\n'.format(bench, time))


def _run_connection_pool(
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
            _record_measurement(bench, time)


def _run_protocol(
    exe_dir: Path,
    iters: int,
) -> None:
    # The benchmarks to run
    benchmarks = [
        "one_small_row_boost",
        "one_small_row_libmysqlclient",
        "one_small_row_libmariadb",
        "one_big_row_boost",
        "one_big_row_libmysqlclient",
        "one_big_row_libmariadb",
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
            _record_measurement(bench, time)


def run_benchmarks(
    exe_dir: Path,
    server_host: str,
    connection_pool_iters: int,
    protocol_iters: int,
) -> None:
    # Truncate the results file, if it exists
    with open(_results_file, 'wt') as f:
        pass

    # Run the benchmarks
    _run_connection_pool(exe_dir, connection_pool_iters, server_host)
    _run_protocol(exe_dir, protocol_iters)
