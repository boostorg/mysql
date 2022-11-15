#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

$ErrorActionPreference = "Stop"
CHCP 65001 
$OutputEncoding = New-Object -typename System.Text.UTF8Encoding
$Env:BOOST_ROOT = "$Env:USERPROFILE\boost"
$Env:BOOST_MYSQL_NO_UNIX_SOCKET_TESTS = "1"
$Env:BOOST_MYSQL_SERVER_HOST = "localhost"
$Env:OPENSSL_ROOT = "C:\Program Files\OpenSSL-Win64"
# $Env:OPENSSL_ROOT = "C:\Program Files (x86)\OpenSSL-Win32"

cd $Env:BOOST_ROOT
.\b2 libs/mysql/test libs/mysql/test//boost_mysql_integrationtests libs/mysql/example//boost_mysql_all_examples address-model=32 -j4
