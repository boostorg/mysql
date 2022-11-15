#
# Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

$ErrorActionPreference = "Stop"

# Set encoding to UTF-8
CHCP 65001 
$OutputEncoding = New-Object -typename System.Text.UTF8Encoding

# Runs a command and checks for a zero exit code
function Check-Call($blk)
{
    Invoke-Command -ScriptBlock $blk
    if ($LastExitCode)
    {
        Write-Host "Command failed: '$blk'"
        exit 1
    }
}

$Env:Path += ";C:\Program Files\MySQL\MySQL Server 5.7\bin"
$Env:Path = "C:\Python37-x64;" + $Env:Path # Override Python 2 setting
$Env:BOOST_MYSQL_NO_UNIX_SOCKET_TESTS = "1"
$Env:BOOST_MYSQL_NO_SHA256_TESTS = "1"
$Env:BOOST_MYSQL_SERVER_HOST = "localhost"

### DB setup
# Copy config files
cp .\tools\win-ci.cnf C:\my.cnf
cp -Recurse .\tools\ssl C:\

# Change root password
Check-Call { mysql -u root -pPassword12! -e "ALTER USER 'root'@'localhost' IDENTIFIED BY ''; FLUSH PRIVILEGES;" }

# Restart MySQL
Restart-Service MySQL57

# Load the data
Get-Content -Encoding utf8 .\test\integration\db_setup.sql | mysql -u root
Get-Content -Encoding utf8 .\example\db_setup.sql | mysql -u root

# Actual build
if ($Env:B2_TOOLSET) # Use Boost.Build
{
    # Definitions based on platform
    if ($Env:PLATFORM -eq "x64")
    {
        $Env:OPENSSL_ROOT = "C:\OpenSSL-v111-Win64"
        $Env:B2_ADDRESS_MODEL = 64
    }
    elseif ($Env:PLATFORM -eq "x86")
    {
        $Env:OPENSSL_ROOT = "C:\OpenSSL-v111-Win32"
        $Env:B2_ADDRESS_MODEL = 32
    }
    else
    {
        Write-Host "Unknown PLATFORM environment variable value: $Env:PLATFORM"
        exit 1
    }
    
    # Boost.Build setup
    Copy-Item -Path "tools\user-config.jam" -Destination "$HOME\user-config.jam"
    
    # Boost.CI install and build
    Check-Call { git clone https://github.com/boostorg/boost-ci.git C:\boost-ci-cloned }
    Copy-Item -Path "C:\boost-ci-cloned\ci" -Destination ".\ci" -Recurse
    Remove-Item -Recurse -Force "C:\boost-ci-cloned"
    Check-Call { .\tools\build_windows_b2.bat }
}
else # Use CMake
{
    $InstallPrefix = "C:\boost_mysql"
    $BoostLocation = "C:\boost_latest"

    # Download and build Boost
    Check-Call { git clone https://github.com/anarthal/boost-windows-mirror.git }
    Set-Location -Path "boost-windows-mirror"
    Check-Call { .\bootstrap.bat }
    Check-Call { .\b2 `
        --prefix=${BoostLocation} `
        --with-system `
        --with-context `
        --with-coroutine `
        --with-date_time `
        --with-test `
        -d0 `
        install
    }
    Set-Location -Path ".."

    # CMake build
    New-Item -Path "." -Name "build" -ItemType "directory" | Out-Null
    Set-Location -Path "build"
    Check-Call { cmake `
        "-G" "Ninja" `
        "-DCMAKE_INSTALL_PREFIX=$InstallPrefix"  `
        "-DCMAKE_PREFIX_PATH=$BoostLocation" `
        "-DCMAKE_BUILD_TYPE=$Env:CMAKE_BUILD_TYPE" `
        "-DCMAKE_CXX_STANDARD=17" `
        "-DCMAKE_C_COMPILER=cl" `
        "-DCMAKE_CXX_COMPILER=cl" `
        "-DBOOST_MYSQL_INTEGRATION_TESTS=ON" `
        ".."
    }
    Check-Call { cmake --build . -j --target install }
    Check-Call { ctest --verbose }
    Check-Call { python `
        ..\tools\user_project_find_package\build.py `
        "-DCMAKE_PREFIX_PATH=$InstallPrefix;$BoostLocation"
    }
}
