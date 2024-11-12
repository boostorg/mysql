#!/usr/bin/python3
#
# Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from typing import NamedTuple, List
from os import path, listdir

REPO_BASE = path.abspath(path.join(path.dirname(path.realpath(__file__)), '..', '..'))

class Example(NamedTuple):
    id: str
    path: str
    title: str


class MultiExample(NamedTuple):
    id: str
    paths: List[str]
    title: str


LINK_TEMPLATE = '* [link mysql.examples.{example.id} {example.title}]'
SECTION_TEMPLATE = '''
[section:{example.id} {example.title}]

This example assumes you have gone through the [link mysql.examples.setup setup].

{example_cpps}

[endsect]
'''

TEMPLATE='''[/
    Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
   
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
]

[/ This file was auto-generated by examples_qbk.py. Do not edit directly ]

[section:examples Examples]

To run the examples, please go through the [link mysql.examples.setup setup] first.

Here is a list of available examples:

[heading Tutorials]

Self-contained programs demonstrating the basic concepts.

{tutorial_links}

[heading Simple programs]

Self-contained programs demonstrating more advanced concepts and techniques.

{simple_links}

[heading Advanced examples]

Programs implementing real-world functionality.

{advanced_links}
# [@https://github.com/anarthal/servertech-chat The BoostServerTech chat project uses Boost.MySQL and Boost.Redis to implement a chat server]

[heading Setup]

To run the examples, you need a MySQL server you can connect to.
Examples make use of a database named `boost_mysql_examples`.
The server hostname and credentials (username and password) are passed 
to the examples via the command line.

You can spin up a server quickly by using Docker:

[!teletype]
```
    # Remove the "-v /var/run/mysqld:/var/run/mysqld" part if you don't need UNIX sockets
    > docker run --name some-mysql -p 3306:3306 -v /var/run/mysqld:/var/run/mysqld -d -e MYSQL_ROOT_PASSWORD= -e MYSQL_ALLOW_EMPTY_PASSWORD=1 -d mysql

    # All the required data can be loaded by running example/db_setup.sql.
    # If you're using the above container, the root user has a blank password
    > mysql -u root < example/db_setup.sql
```

Please note that this container is just for demonstrative purposes,
and is not suitable for production.

The root MySQL user for these containers is `root` and has an empty password.

{all_examples}

[endsect]

'''

# List all examples here
TUTORIALS = [
    Example('tutorial_sync', '1_tutorial/1_sync.cpp', 'Tutorial 1 listing: hello world!'),
    Example('tutorial_async', '1_tutorial/2_async.cpp', 'Tutorial 2 listing: going async with C++20 coroutines'),
    Example('tutorial_with_params', '1_tutorial/3_with_params.cpp', 'Tutorial 3 listing: queries with parameters'),
    Example('tutorial_static_interface', '1_tutorial/4_static_interface.cpp', 'Tutorial 4 listing: the static interface'),
    Example('tutorial_updates_transactions', '1_tutorial/5_updates_transactions.cpp', 'Tutorial 5 listing: UPDATEs, transactions and multi-queries'),
]

SIMPLE_EXAMPLES = [
    Example('inserts', '2_simple/inserts.cpp', 'INSERTs, last_insert_id() and NULL values'),
    Example('deletes', '2_simple/deletes.cpp', 'DELETEs and affected_rows()'),
    Example('prepared_statements', '2_simple/prepared_statements.cpp', 'Prepared statements'),
    Example('timeouts', '2_simple/timeouts.cpp', 'Setting timeouts to operations'),
    Example('disable_tls', '2_simple/disable_tls.cpp', 'Disabling TLS for a connection'),
    Example('tls_certificate_verification', '2_simple/tls_certificate_verification.cpp', 'Setting TLS options: enabling TLS certificate verification'),
    Example('metadata', '2_simple/metadata.cpp', 'Metadata'),
    Example('multi_function', '2_simple/multi_function.cpp', 'Reading rows in batches with multi-function operations'),
    Example('callbacks', '2_simple/callbacks.cpp', 'Callbacks (async functions in C++11)'),
    Example('coroutines_cpp11', '2_simple/coroutines_cpp11.cpp', 'Stackful coroutines (async functions in C++11)'),
    Example('unix_socket', '2_simple/unix_socket.cpp', 'UNIX sockets'),
    Example('batch_inserts', '2_simple/batch_inserts.cpp', 'Batch inserts using client-side query formatting'),
    Example('batch_inserts_generic', '2_simple/batch_inserts_generic.cpp', 'Generic batch inserts with Boost.Describe'),
    Example('dynamic_filters', '2_simple/dynamic_filters.cpp', 'Queries with dynamic filters'),
    Example('patch_updates', '2_simple/patch_updates.cpp', 'Dynamic UPDATE queries with PATCH-like semantics'),
    Example('source_script', '2_simple/source_script.cpp', 'Sourcing a .sql file using multi-queries'),
    Example('pipeline', '2_simple/pipeline.cpp', '(Experimental) Pipelines'),
]

ADVANCED_EXAMPLES = [
    MultiExample('connection_pool', [
        '3_advanced/connection_pool/main.cpp',
        '3_advanced/connection_pool/types.hpp',
        '3_advanced/connection_pool/repository.hpp',
        '3_advanced/connection_pool/repository.cpp',
        '3_advanced/connection_pool/handle_request.hpp',
        '3_advanced/connection_pool/handle_request.cpp',
        '3_advanced/connection_pool/server.hpp',
        '3_advanced/connection_pool/server.cpp',
        '3_advanced/connection_pool/log_error.hpp',
    ], 'A REST API server that uses connection pooling')
]

ALL_EXAMPLES = TUTORIALS + SIMPLE_EXAMPLES + ADVANCED_EXAMPLES

def _render_links(examples: List[Example]) -> str:
    return '\n'.join(LINK_TEMPLATE.format(example=elm) for elm in examples)

def _write_file(relpath: List[str], contents: str) -> None:
    output_file = path.join(REPO_BASE, *relpath)
    with open(output_file, 'wt') as f:
        f.write(contents)

def _render_simple_cpp(id: str) -> str:
    return f'[example_{id}]'

def _render_multi_cpp(id: str, paths: List[str]) -> str:
    def get_file_id(p: str):
        # File IDs follow the below convention
        converted_id = path.basename(p).replace('.', '_')
        return f'{id}_{converted_id}'

    return '\n\n'.join(_render_simple_cpp(get_file_id(p)) for p in paths)

def _collect_snippets() -> List[str]:
    snippets_relpath = ['test', 'integration', 'test', 'snippets']
    return [
        path.join(*snippets_relpath, p)
        for p in  listdir(path.join(REPO_BASE, *snippets_relpath))
    ]

def _replace_imports(import_contents: str) -> None:
    # Read the file
    with open(path.join(REPO_BASE, 'doc', 'qbk', '00_main.qbk'), 'rt') as f:
        contents = f.read()
    
    # Replace
    begin_marker = '[/ AUTOGENERATED IMPORTS BEGIN ]\n'
    end_marker = '\n[/ AUTOGENERATED IMPORTS END ]'
    begin_pos = contents.find(begin_marker)
    end_pos = contents.find(end_marker)
    assert begin_pos != -1
    assert end_pos != -1
    final_contents = contents[:begin_pos + len(begin_marker)] + import_contents + contents[end_pos:]

    # Write the file
    _write_file(['doc', 'qbk', '00_main.qbk'], final_contents)


def main():
    # Collect all files to be imported
    example_paths = [e.path for e in ALL_EXAMPLES if isinstance(e, Example)]
    for p in [e.paths for e in ALL_EXAMPLES if isinstance(e, MultiExample)]:
        example_paths += p
    all_paths = [f'example/{p}' for p in example_paths]
    all_paths += _collect_snippets()
    

    # Render
    import_contents = '\n'.join(f'[import ../../{p}]' for p in all_paths)
    example_contents = TEMPLATE.format(
        tutorial_links=_render_links(TUTORIALS),
        simple_links=_render_links(SIMPLE_EXAMPLES),
        advanced_links='',
        all_examples='\n\n\n'.join(SECTION_TEMPLATE.format(
            example=elm,
            example_cpps=_render_multi_cpp(elm.id, elm.paths) if isinstance(elm, MultiExample) else _render_simple_cpp(elm.id)
        ) for elm in ALL_EXAMPLES)
    )

    # Write to output file
    _replace_imports(import_contents)
    _write_file(['doc', 'qbk', '21_examples.qbk'], example_contents)
    

if __name__ == '__main__':
    main()