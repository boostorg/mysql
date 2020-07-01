REM
REM Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
REM
REM Distributed under the Boost Software License, Version 1.0. (See accompanying
REM file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
REM

call ci\appveyor\install.bat || exit /b 1
call ci\build.bat || exit /b 1