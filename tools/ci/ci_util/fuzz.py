#!/usr/bin/python3
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from pathlib import Path
import os
from shutil import unpack_archive, make_archive
from .common import install_boost, BOOST_ROOT, run
from .seed_corpus import generate_seed_corpus


def fuzz_build(
    source_dir: Path,
    clean: bool = False,
    boost_branch: str = 'develop',
) -> None:
    # Config
    os.environ['UBSAN_OPTIONS'] = 'print_stacktrace=1'

    # Get Boost. This leaves us inside boost root
    install_boost(
        BOOST_ROOT,
        source_dir=source_dir,
        clean=clean,
        branch=boost_branch
    )

    # Setup corpus from previous runs. /tmp/corpus.tar.gz is restored by the CI
    old_corpus = Path('/tmp/corpus.tar.gz')
    if old_corpus.exists():
        print('+  Restoring old corpus')
        unpack_archive(old_corpus, extract_dir='/tmp/corpus')
    else:
        print('+  No old corpus found')
    
    # Setup the seed corpus
    print('+  Generating seed corpus')
    generate_seed_corpus()

    # Build and run the fuzzing targets
    run([
        'b2',
        '--abbreviate-paths',
        'toolset=clang',
        'cxxstd=20',
        'warnings-as-errors=on',
        '-j4',
        'libs/mysql/test/fuzzing',
    ])

    # Archive the generated corpus, so the CI caches it
    name = make_archive(str(old_corpus).rstrip('.tar.gz'), 'gztar', '/tmp/mincorpus')
    print('  + Created min corpus archive {}'.format(name))
    
