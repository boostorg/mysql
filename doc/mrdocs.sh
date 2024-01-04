#!/bin/bash
#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

repo=$HOME/workspace/mysql

set -e

/opt/mrdocs/bin/mrdocs $repo/__build/compile_commands.json --config=$repo/doc/mrdocs.yml --output=$repo/doc/antora/mysql/modules/reference/pages/
cd $repo/doc/antora
npx antora antora-playbook.yml
