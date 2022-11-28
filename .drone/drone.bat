REM
REM Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
REM
REM Distributed under the Boost Software License, Version 1.0. (See accompanying
REM file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
REM

setlocal enabledelayedexpansion

if "%DRONE_JOB_BUILDTYPE%" == "b2" (
    call tools\build_windows_b2.bat
) else if "%DRONE_JOB_BUILDTYPE%" == "cmake" (
    call tools\build_windows_cmake.bat
) else (
    echo "Unknown DRONE_JOB_BUILDTYPE: %DRONE_JOB_BUILDTYPE%"
    exit /b 1
)
