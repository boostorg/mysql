#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

$ErrorActionPreference = "Stop"

$Env:B2_TOOLSET = "msvc"
$Env:B2_CXXSTD = "11,14,17"
$Env:B2_ADDRESS_MODEL = "32"
$Env:B2_VARIANT = "debug,release"
$Env:USE_SYMLINK = "1"

# The CI script expect us to be in the source directory
cd "C:\boost-mysql"

# Symlink handling
# if (Test-Path "C:\boost-root\libs\mysql") {
#     (Get-Item "C:\boost-root\libs\mysql").Delete()
# }
# if (Test-Path "C:\boost-root") {
#     Remove-Item -Recurse -Force "C:\boost-root"
# }
cmd /q /c tools\build_windows_b2.bat
