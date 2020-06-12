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
    
    # Valgrind tests and Valgrind-friendly code (e.g. mark SSL buffers as initialized)
    option(BOOST_MYSQL_VALGRIND_TESTS OFF "Whether to run Valgrind tests or not (requires Valgrind)")
    mark_as_advanced(BOOST_MYSQL_VALGRIND_TESTS)
    
    # Build with coverage
    option(BOOST_MYSQL_COVERAGE OFF "Whether to build using coverage")
    mark_as_advanced(BOOST_MYSQL_COVERAGE)
    
    # Run SHA256 tests
    option(BOOST_MYSQL_SHA256_TESTS OFF "Whether to run SHA256 tests or not")
    mark_as_advanced(BOOST_MYSQL_SHA256_TESTS)
endif()

option(BOOST_MYSQL_ALLOW_FETCH_CONTENT ON "Allow FetchContent for date library")
mark_as_advanced(BOOST_MYSQL_ALLOW_FETCH_CONTENT)

# Includes
include(GNUInstallDirs)
include(FetchContent)

# Dependencies
find_package(Boost 1.72.0 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
if (NOT BOOST_MYSQL_ALLOW_FETCH_CONTENT)
    find_package(date REQUIRED)
else()
    find_package(date QUIET)
endif()

if (NOT date_FOUND)
    message("-- Could NOT find date, fetching it")
    FetchContent_Declare(
        date
        GIT_REPOSITORY https://github.com/HowardHinnant/date.git
        GIT_TAG        v2.4.1
    )
    
    FetchContent_GetProperties(date)
    if(NOT date_POPULATED)
        FetchContent_Populate(date)
        set(ENABLE_DATE_TESTING OFF CACHE BOOL "")
        set(USE_SYSTEM_TZ_DB ON CACHE BOOL "")
        add_subdirectory(${date_SOURCE_DIR} ${date_BINARY_DIR})
    endif()
endif()

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
    date_interface
)
target_include_directories(
    boost_mysql
    INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(
    boost_mysql
    INTERFACE
    cxx_std_11
)

# If we're on MSVC, the value of the __cplusplus macro is incorrect.
# We could fix it using /Zc:__cplusplus, but that breaks date.h
# Asio detects default function template arguments using __cplusplus,
# which are required for default completion tokens. This feature is
# available on every C++11 or higher capable compiler. Force it.
if (MSVC)
    target_compile_definitions(
        boost_mysql
        INTERFACE
        BOOST_ASIO_HAS_DEFAULT_FUNCTION_TEMPLATE_ARGUMENTS=1
    )
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
