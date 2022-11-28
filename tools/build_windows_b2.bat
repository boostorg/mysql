REM
REM Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
REM
REM Distributed under the Boost Software License, Version 1.0. (See accompanying
REM file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
REM

REM Config
SET SOURCE_DIR="%cd%"
SET BOOST_ROOT=C:\boost-root
SET OPENSSL_ROOT=C:\openssl-%B2_ADDRESS_MODEL%
SET PATH=%BOOST_ROOT%;%PATH%
SET BOOST_MYSQL_NO_UNIX_SOCKET_TESTS=1
SET BOOST_MYSQL_SERVER_HOST=localhost

REM Get Boost
call tools\install_boost.bat
cd "%BOOST_ROOT%" || exit /b 1

REM Build
.\b2 --abbreviate-paths ^
    toolset=%B2_TOOLSET% ^
    cxxstd=%B2_CXXSTD% ^
    address-model=%B2_ADDRESS_MODEL% ^
    variant=%B2_VARIANT% ^
    -j4 ^
    libs/mysql/test ^
    libs/mysql/test//boost_mysql_integrationtests ^
    libs/mysql/example//boost_mysql_all_examples || exit /b 1
