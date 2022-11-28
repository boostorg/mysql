REM
REM Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)

REM BOOST_ROOT: required, directory where boost should be located
REM SOURCE_DIR: required, path to our library
REM USE_SYMLINK: optional, whether to copy mysql or use symlinks

@echo off
if not exist %BOOST_ROOT% (

    REM Clone Boost
    git clone -b master --depth 1 https://github.com/boostorg/boost.git %BOOST_ROOT% || exit /b 1
    cd %BOOST_ROOT%  || exit /b 1

    REM Put our library inside boost root. Symlinks are used during local development
    if "%USE_SYMLINK%" == "1" (
        mklink /d %BOOST_ROOT%\libs\mysql %SOURCE_DIR% || exit /b 1
    ) else (
        mkdir libs\mysql || exit /b 1
        xcopy /s /e /q /I %SOURCE_DIR%\* %BOOST_ROOT%\libs\mysql\ || exit /b 1
    )

    REM Install Boost dependencies
    git config submodule.fetchJobs 8 || exit /b 1 REM If supported, this will accelerate depinst
    git submodule update -q --init tools/boostdep || exit /b 1
    python tools/boostdep/depinst/depinst.py --include example mysql || exit /b 1

    REM Booststrap
    .\bootstrap.bat || exit /b 1
    .\b2 headers || exit /b 1

)
