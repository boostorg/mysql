REM
REM Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
REM
REM Distributed under the Boost Software License, Version 1.0. (See accompanying
REM file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
REM

SETLOCAL
SET IMAGE=build-msvc14_3
SET TYPE=cmake
@REM SET TYPE=b2

SET CONTAINER="builder-%IMAGE%"
docker start %CONTAINER% || docker run -dit --name %CONTAINER% -v "%USERPROFILE%\mysql:C:\boost-mysql" "ghcr.io/anarthal-containers/%IMAGE%" || exit /b 1
docker exec %CONTAINER% powershell.exe "C:\boost-mysql\tools\scripts\build_windows_%TYPE%_local_docker.ps1" || exit /b 1

ENDLOCAL
