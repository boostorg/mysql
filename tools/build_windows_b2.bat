REM
REM Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
REM
REM Distributed under the Boost Software License, Version 1.0. (See accompanying
REM file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
REM

call ci\appveyor\install.bat || exit /b 1

REM Copied from boost-ci to enable argument forwarding
setlocal enabledelayedexpansion

IF NOT DEFINED B2_CI_VERSION (
    echo You need to set B2_CI_VERSION in your CI script
    exit /B 1
)

PATH=%ADDPATH%%PATH%

SET B2_TOOLCXX=toolset=%B2_TOOLSET%

IF DEFINED B2_CXXSTD (SET B2_CXXSTD=cxxstd=%B2_CXXSTD%)
IF DEFINED B2_CXXFLAGS (SET B2_CXXFLAGS=cxxflags=%B2_CXXFLAGS%)
IF DEFINED B2_DEFINES (SET B2_DEFINES=define=%B2_DEFINES%)
IF DEFINED B2_ADDRESS_MODEL (SET B2_ADDRESS_MODEL=address-model=%B2_ADDRESS_MODEL%)
IF DEFINED B2_LINK (SET B2_LINK=link=%B2_LINK%)
IF DEFINED B2_VARIANT (SET B2_VARIANT=variant=%B2_VARIANT%)

cd %BOOST_ROOT%

IF DEFINED SCRIPT (
    call libs\%SELF%\%SCRIPT%
) ELSE (
    set SELF_S=%SELF:\=/%
    REM Echo the complete build command to the build log
    ECHO b2 --abbreviate-paths libs/!SELF_S!/test %B2_TOOLCXX% %B2_CXXSTD% %B2_CXXFLAGS% %B2_DEFINES% %B2_THREADING% %B2_ADDRESS_MODEL% %B2_LINK% %B2_VARIANT% -j4 %*
    REM Now go build...
    b2 --abbreviate-paths libs/!SELF_S!/test %B2_TOOLCXX% %B2_CXXSTD% %B2_CXXFLAGS% %B2_DEFINES% %B2_THREADING% %B2_ADDRESS_MODEL% %B2_LINK% %B2_VARIANT% -j4 %*
)
