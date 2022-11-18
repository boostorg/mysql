#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#


globalenv={'B2_CI_VERSION': '1', 'B2_VARIANT': 'release'}
linuxglobalimage="cppalliance/droneubuntu1804:1"
windowsglobalimage="cppalliance/dronevs2019"

def main(ctx):
  return [
    windows_cxx("MSVC 14.1 (Win32)", "", image="cppalliance/dronevs2017", buildtype="boost", buildscript="drone", environment={"B2_TOOLSET": "msvc-14.1", "B2_ADDRESS_MODEL": '32', "B2_CXXSTD": "11,14,17"},globalenv=globalenv),
    windows_cxx("MSVC 14.1", "", image="cppalliance/dronevs2017", buildtype="boost", buildscript="drone", environment={"B2_TOOLSET": "msvc-14.1", "B2_CXXSTD": "11,14,17"},globalenv=globalenv),
    windows_cxx("MSVC 14.2: C++14,17,latest", "", image="cppalliance/dronevs2019", buildtype="boost", buildscript="drone", environment={"B2_TOOLSET": "msvc-14.2", "B2_CXXSTD": "14,17,latest"},globalenv=globalenv),
    windows_cxx("MSVC 14.3: C++17,20", "", image="cppalliance/dronevs2022", buildtype="boost", buildscript="drone", environment={"B2_TOOLSET": "msvc-14.3", "B2_CXXSTD": "17,20"},globalenv=globalenv),
]

# from https://github.com/boostorg/boost-ci
load("@boost_ci//ci/drone/:functions.star", "linux_cxx","windows_cxx","osx_cxx","freebsd_cxx")