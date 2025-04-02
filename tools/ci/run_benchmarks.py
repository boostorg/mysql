#!/usr/bin/python3
#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# This script is a utility, and not used by CI. Having it here simplifies imports, though.

from ci_util.bench import run_benchmarks
from pathlib import Path
import sys

if __name__ == '__main__':
    run_benchmarks(
        exe_dir=Path(sys.argv[1]),
        server_host='localhost',
        connection_pool_iters=0,
        protocol_iters=25
    )
