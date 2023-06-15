#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# This may be called from add_subdirectory, too
if(BOOST_MYSQL_IS_ROOT)
    find_package(Boost ${BOOST_MYSQL_VERSION} REQUIRED COMPONENTS unit_test_framework)
endif()

# boost_mysql_testing contains common definitions, includes and settings
add_library(
    boost_mysql_testing
    INTERFACE
)
target_sources(
    boost_mysql_testing
    INTERFACE
    common/src/entry_point.cpp
    common/src/tracker_executor.cpp
)

# Asio static library to speed up compilation
if(BOOST_MYSQL_IS_ROOT)
    target_link_libraries(
        boost_mysql_testing
        INTERFACE
        Boost::headers
        Boost::unit_test_framework
        Boost::disable_autolinking
    )
else()
    target_link_libraries(
        boost_mysql_testing
        INTERFACE
        Boost::asio
        Boost::optional
        Boost::unit_test_framework
    )
endif()

target_compile_features(
    boost_mysql_testing
    INTERFACE
    cxx_std_11
)
target_include_directories(
    boost_mysql_testing
    INTERFACE
    common/include
)

# Unit testing
add_subdirectory(unit)
add_subdirectory(unit_iface)

# Integration testing
if(BOOST_MYSQL_INTEGRATION_TESTS)
    add_subdirectory(integration)
endif()
