#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# This may be called from add_subdirectory, too
if (BOOST_MYSQL_IS_ROOT)
    find_package(Boost ${BOOST_MYSQL_VERSION} REQUIRED COMPONENTS unit_test_framework)
endif()

# boost_mysql_testing is a static lib that contains some utilities,
# asio as separate source and the UTF main function. Used to speed up test compilation
add_library(
    boost_mysql_testing
    STATIC
   common/src/asio.cpp
   common/src/tracker_executor.cpp
   common/src/utf_entry_point.cpp
)
boost_mysql_common_target_settings(boost_mysql_testing)


# Asio static library to speed up compilation
if (BOOST_MYSQL_IS_ROOT)
    target_link_libraries(
        boost_mysql_testing
        PUBLIC
        Boost::headers
        Boost::unit_test_framework
        Boost::disable_autolinking
    )
else()
    target_link_libraries(
        boost_mysql_testing
        PUBLIC
        Boost::asio
        Boost::optional
        Boost::unit_test_framework
    )
endif()

target_link_libraries(
    boost_mysql_testing
    PUBLIC
    OpenSSL::Crypto
    OpenSSL::SSL
)
target_compile_definitions(
    boost_mysql_testing
    PUBLIC
    BOOST_ASIO_SEPARATE_COMPILATION
)
target_compile_features(
    boost_mysql_testing
    PUBLIC
    cxx_std_11
)
target_include_directories(
    boost_mysql_testing
    PUBLIC
    common/include
)

# Unit testing
add_subdirectory(unit)
add_subdirectory(unit_iface)

# Integration testing
if(BOOST_MYSQL_INTEGRATION_TESTS)
    add_subdirectory(integration)
endif()
