#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

def main(ctx):
  return [
    windows_cxx("MSVC 14.1", "", image="cppalliance/dronevs2017", buildtype="b2", buildscript="drone", environment={
      "B2_TOOLSET": "msvc-14.1",
      "B2_CXXSTD": "11,14,17",
      "B2_VARIANT": "release",
      "B2_ADDRESS_MODEL": "64"
    }),
    windows_cxx("MSVC 14.2", "", image="cppalliance/dronevs2019", buildtype="b2", buildscript="drone", environment={
      "B2_TOOLSET": "msvc-14.2",
      "B2_CXXSTD": "14,17",
      "B2_VARIANT": "release",
      "B2_ADDRESS_MODEL": "64"
    }),
    windows_cxx("MSVC 14.3", "", image="cppalliance/dronevs2022", buildtype="b2", buildscript="drone", environment={
      "B2_TOOLSET": "msvc-14.3",
      "B2_CXXSTD": "17,20",
      "B2_VARIANT": "debug,release",
      "B2_ADDRESS_MODEL": "64"
    }),
    windows_cxx("MSVC 14.3, 32-bit", "", image="cppalliance/dronevs2022", buildtype="b2", buildscript="drone", environment={
      "B2_TOOLSET": "msvc-14.3",
      "B2_CXXSTD": "20",
      "B2_VARIANT": "debug,release",
      "B2_ADDRESS_MODEL": "32"
    })
  ]

load("@boost_ci//ci/drone/:functions.star", "windows_cxx")
