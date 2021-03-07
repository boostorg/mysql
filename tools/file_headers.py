#!/usr/bin/python3
#
# Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

import os
import re
from os import path
from collections import namedtuple

# Script to get file headers (copyright notices
# and include guards) okay and up to date

REPO_BASE = path.abspath(path.join(path.dirname(__file__), '..'))
BASE_FOLDERS = [
    'cmake',
    'doc',
    'example',
    'include',
    'test',
    'tools'
]
BASE_FILES = [
    '.appveyor.yml',
    '.travis.yml',
    'CMakeLists.txt',
    'Jamfile'
]
HTML_GEN_PATH = path.join(REPO_BASE, 'doc', 'html')

HEADER_TEMPLATE = '''{begin}
{linesym} Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
{linesym}
{linesym} Distributed under the Boost Software License, Version 1.0. (See accompanying
{linesym} file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
{end}'''

MYSQL_ERROR_HEADER = '/usr/include/mysql/mysqld_error.h'

def find_first_blank(lines):
    return [i for i, line in enumerate(lines) if line == ''][0]

def read_file(fpath):
    with open(fpath, 'rt') as f:
        return f.readlines()
    
def write_file(fpath, lines):
    with open(fpath, 'wt') as f:
        f.writelines(lines)

def text_to_lines(text):
    return [line + '\n' for line in text.split('\n')]

def gen_header(linesym, opensym=None, closesym=None, shebang=None, include_guard=None):
    opensym = linesym if opensym is None else opensym
    closesym = linesym if closesym is None else closesym
    if shebang is None:
        begin = opensym
    else:
        begin = shebang + '\n' + opensym
    if include_guard is None:
        end = closesym
    else:
        end = closesym + '\n\n#ifndef {0}\n#define {0}'.format(include_guard)
    return text_to_lines(HEADER_TEMPLATE.format(begin=begin, end=end, linesym=linesym))

class NormalProcessor(object):
    def __init__(self, name, header):
        self.header = header
        self.name = name
        
    def process(self, fpath):
        lines = read_file(fpath)
        first_blank = find_first_blank(line.replace('\n', '') for line in lines)
        lines = self.header + lines[first_blank:]
        write_file(fpath, lines)
        
class HppProcessor(object):
    name = 'hpp'
    
    def process(self, fpath):
        lines = read_file(fpath)
        first_content = [i for i, line in enumerate(lines) if line.startswith('#define')][0] + 1
        iguard = self._gen_include_guard(fpath)
        header = gen_header('//', include_guard=iguard)
        lines = header + lines[first_content:]
        write_file(fpath, lines)
        
        
    @staticmethod
    def _gen_include_guard(fpath):
        include_base = path.join(REPO_BASE, 'include')
        if fpath.startswith(include_base):
            relpath = path.relpath(fpath, include_base)
        else:
            relpath = path.join('boost', 'mysql', path.relpath(fpath, REPO_BASE))
        return relpath.replace('/', '_').replace('.', '_').upper()
        
class XmlProcessor(object):
    name = 'xml'
    header = gen_header('   ', '<!--', '-->')
    
    def process(self, fpath):
        lines = read_file(fpath)
        if lines[0].startswith('<?'):
            first_blank = [i for i, line in enumerate(lines) if line.strip() == ''][0]
            first_content = [i for i, line in enumerate(lines[first_blank:]) \
                             if line.startswith('<') and not line.startswith('<!--')][0] + first_blank
            lines = lines[0:first_blank] + ['\n'] + self.header + ['\n'] + lines[first_content:]
            write_file(fpath, lines)
        else:
            NormalProcessor('xml', self.header).process(fpath)
        
        
class IgnoreProcessor(object):
    name = 'ignore'
    
    def process(self, fpath):
        pass
        
hash_processor = NormalProcessor('hash', gen_header('#'))
qbk_processor = NormalProcessor('qbk', gen_header('   ', opensym='[/', closesym=']'))
sql_processor = NormalProcessor('sql', gen_header('--'))
cpp_processor = NormalProcessor('cpp', gen_header('//'))
py_processor = NormalProcessor('py', gen_header('#', shebang='#!/usr/bin/python3'))
bash_processor = NormalProcessor('bash', gen_header('#', shebang='#!/bin/bash'))
bat_processor = NormalProcessor('bat', gen_header('REM'))

FILE_PROCESSORS = [
    ('docca-base-stage2-noescape.xsl', IgnoreProcessor()),
    ('CMakeLists.txt', hash_processor),
    ('.cmake', hash_processor),
    ('.cmake.in', hash_processor),
    ('Jamfile', hash_processor),
    ('.jam', hash_processor),
    ('Doxyfile', hash_processor),
    ('.qbk', qbk_processor),
    ('.sql', sql_processor),
    ('.py', py_processor),
    ('.sh', bash_processor),
    ('.bat', bat_processor),
    ('.ps1', hash_processor),
    ('.yml', hash_processor),
    ('.cnf', hash_processor),
    ('.dockerfile', hash_processor),
    ('.cpp', cpp_processor),
    ('.hpp', HppProcessor()),
    ('.ipp', HppProcessor()),
    ('.xml', XmlProcessor()),
    ('.xsl', XmlProcessor()),
    ('.svg', IgnoreProcessor()),
    ('valgrind_suppressions.txt', IgnoreProcessor()),
    ('.pem', IgnoreProcessor()),
]

def process_file(fpath):
    for ext, processor in FILE_PROCESSORS:
        if fpath.endswith(ext):
            print('Processing file {} with processor {}'.format(fpath, processor.name))
            processor.process(fpath)
            break
    else:
        raise ValueError('Could not find a suitable processor for file: ' + fpath)
    
def process_all_files():
    res = list(BASE_FILES)
    for base_folder in BASE_FOLDERS:
        base_folder_abs = path.join(REPO_BASE, base_folder)
        for curdir, _, files in os.walk(base_folder_abs):
            if curdir.startswith(HTML_GEN_PATH):
                print('Ignored directory {}'.format(curdir))
                continue
            for fname in files:
                process_file(path.join(curdir, fname))
                
Error = namedtuple('Error', ('symbol', 'number', 'descr', 'is_server'))

ERRC_TEMPLATE = '''
#ifndef BOOST_MYSQL_ERRC_HPP
#define BOOST_MYSQL_ERRC_HPP

#include <iosfwd>

namespace boost {{
namespace mysql {{

/**
 * \\brief MySQL-specific error codes.
 * \\details Some error codes are defined by the client library, and others
 * are returned from the server. For the latter, the numeric value and
 * string descriptions match the ones described in the MySQL documentation.
 * See [mysqllink server-error-reference.html the MySQL error reference]
 * for more info on server errors.
 */
enum class errc : int
{{
{}
}};

/**
  * \\relates errc
  * \\brief Streams an error code.
  */
inline std::ostream& operator<<(std::ostream&, errc);

}} // mysql
}} // boost

#endif
'''

DESCRIPTIONS_TEMPLATE='''
#ifndef BOOST_MYSQL_IMPL_ERROR_DESCRIPTIONS_HPP
#define BOOST_MYSQL_IMPL_ERROR_DESCRIPTIONS_HPP

#include "boost/mysql/errc.hpp"

namespace boost {{
namespace mysql {{
namespace detail {{

struct error_entry
{{
    errc value;
    const char* message;
}};

constexpr error_entry all_errors [] = {{
{}
}};

}} // detail
}} // mysql
}} // boost

#endif
'''

def generate_errc_entry(err):
    if err.is_server:
        doc = ('Server error. Error number: {}, symbol: ' + \
              '[mysqllink server-error-reference.html\\#error_er_{} ER_{}].').format(
                  err.number, err.symbol, err.symbol.upper())
    else:
        if err.number == 0:
            doc = err.descr
        else:
            doc = 'Client error. ' + err.descr
    return '    {} = {}, ///< {}'.format(err.symbol, err.number, doc)

def generate_description_entry(err):
    return '    {{ errc::{}, "{}" }},'.format(err.symbol, err.descr)
                
def generate_error_enums():
    # Get the error list
    with open(MYSQL_ERROR_HEADER, 'rt') as f:
        content = f.read()
    pat = r'#define ER_([A-Z0-9_]*) ([0-9]*)'
    server_errors = [(symbol.lower(), int(number)) for symbol, number in re.findall(pat, content) if int(number) < 5000]
    client_errors = [
        ('incomplete_message', 65536, 'An incomplete message was received from the server'),
        ('extra_bytes', 65537, 'Unexpected extra bytes at the end of a message were received'),
        ('sequence_number_mismatch', 65538, 'Mismatched sequence numbers'),
        ('server_unsupported', 65539, 'The server does not support the minimum required capabilities to establish the connection'),
        ('protocol_value_error', 65540, 'An unexpected value was found in a server-received message'),
        ('unknown_auth_plugin', 65541, 'The user employs an authentication plugin not known to this library'),
        ('auth_plugin_requires_ssl', 65542, 'The authentication plugin requires the connection to use SSL'),
        ('wrong_num_params', 65543, 'The number of parameters passed to the prepared statement does not match the number of actual parameters'),
    ]
    errors = [Error('ok', 0, 'No error', False)] + \
        [Error(sym, num, sym, True) for (sym, num) in server_errors] + \
        [Error(sym, num, descr, False) for (sym, num, descr) in client_errors]
    
    # Generate errc header
    errc_content = ERRC_TEMPLATE.format('\n'.join(generate_errc_entry(err) for err in errors))
    with open(path.join(REPO_BASE, 'include', 'boost', 'mysql', 'errc.hpp'), 'wt') as f:
        f.write(errc_content)
        
    # Generate error descriptions header
    descr_content = DESCRIPTIONS_TEMPLATE.format('\n'.join(generate_description_entry(err) 
                                                           for err in errors))
    with open(path.join(REPO_BASE, 'include', 'boost', 'mysql', 
                        'impl', 'error_descriptions.hpp'), 'wt') as f:
        f.write(descr_content)
    
            
def main():
    generate_error_enums()
    process_all_files()
            
            
if __name__ == '__main__':
    main()
        
