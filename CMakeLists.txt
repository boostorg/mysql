#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.13.0) # Determined by requiring Boost 1.70
project(boost_mysql VERSION 0.0.1)

# If we are the top-level project, enable testing
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        set(_TESTING_ENABLED ON)
    endif()

    # Don't run integration testing unless explicitly requested
    option(BOOST_MYSQL_INTEGRATION_TESTS OFF "Whether to build and run integration tests or not")
    mark_as_advanced(BOOST_MYSQL_INTEGRATION_TESTS)

    # Valgrind tests and Valgrind-friendly code (e.g. mark SSL buffers as initialized)
    option(BOOST_MYSQL_VALGRIND_TESTS OFF "Whether to run Valgrind tests or not (requires Valgrind)")
    mark_as_advanced(BOOST_MYSQL_VALGRIND_TESTS)
    
    # Build with coverage
    option(BOOST_MYSQL_COVERAGE OFF "Whether to build using coverage")
    mark_as_advanced(BOOST_MYSQL_COVERAGE)
endif()

# Includes
include(GNUInstallDirs)

# Dependencies
find_package(Boost 1.72.0 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Interface library (header-only)
add_library(boost_mysql INTERFACE)
add_library(Boost::mysql ALIAS boost_mysql)

target_link_directories(
    boost_mysql 
    INTERFACE
    ${Boost_LIBRARY_DIRS} # Prevent link errors in Windows
)
target_link_libraries(
    boost_mysql
    INTERFACE
    Boost::system
    Threads::Threads
    OpenSSL::Crypto
    OpenSSL::SSL
)
target_include_directories(
    boost_mysql
    SYSTEM # TODO: review this
    INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(
    boost_mysql
    INTERFACE
    cxx_std_11
)

# Asio bases C++ feature detection on __cplusplus. Make MSVC
# define it correctly
if (MSVC)
    target_compile_options(boost_mysql INTERFACE /Zc:__cplusplus)
endif()

# Installing
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set_target_properties(boost_mysql PROPERTIES EXPORT_NAME mysql)
    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ 
        DESTINATION include
    )
    install(
        TARGETS boost_mysql
        EXPORT boost_mysql_targets
    )
    install(
        EXPORT boost_mysql_targets
        FILE boost_mysql-targets.cmake
        NAMESPACE Boost::
        DESTINATION lib/cmake/boost_mysql
    )
    
    include(CMakePackageConfigHelpers)

    configure_package_config_file(
        cmake/config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/boost_mysql-config.cmake
        INSTALL_DESTINATION lib/cmake/boost_mysql
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/boost_mysql-config-version.cmake
        VERSION ${PROJECT_VERSION} 
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/boost_mysql-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/boost_mysql-config-version.cmake
        DESTINATION lib/cmake/boost_mysql
    )
endif()

# Examples and tests
if(_TESTING_ENABLED)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_utils.cmake)
    add_subdirectory(example)
    add_subdirectory(test)
endif()
