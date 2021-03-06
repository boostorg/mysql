#!/bin/bash
#
# Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

if which llvm-cov; then
    LLVM_COV=llvm-cov
else
    CLANG_VERSION_MAJOR=$(clang --version | head -1 | cut -d ' ' -f 3 | cut -d . -f 1)
    LLVM_COV=llvm-cov-$CLANG_VERSION_MAJOR        
fi

$LLVM_COV gcov "$@"