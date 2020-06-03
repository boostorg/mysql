#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

find_package(Python3 REQUIRED)

# Utility function to set warnings and other compile properties of
# our test targets
function(common_target_settings TARGET_NAME)
    if(MSVC)
        if (WIN32 AND CMAKE_SYSTEM_VERSION)
            set(WINNT_VERSION ${CMAKE_SYSTEM_VERSION})
            string(REPLACE "." "" WINNT_VERSION ${WINNT_VERSION})
            string(REGEX REPLACE "([0-9])" "0\\1" WINNT_VERSION ${WINNT_VERSION})
    
            set(WINNT_VERSION "0x${WINNT_VERSION}")
        endif()
        target_compile_definitions(
            ${TARGET_NAME}
            PRIVATE
            _CRT_SECURE_NO_WARNINGS
            _WIN32_WINNT=${WINNT_VERSION} # Silence warnings in Windows
            _SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING # Warnings in C++17 for Asio
        )
        target_compile_options(${TARGET_NAME} PRIVATE /bigobj) # Prevent failures on Windows
    else()
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -pedantic -Werror)
    endif()
    
    set_target_properties(${TARGET_NAME} PROPERTIES CXX_EXTENSIONS OFF) # disable extensions
    
    # Valgrind
    if (BOOST_MYSQL_VALGRIND_TESTS)
        target_include_directories(${TARGET_NAME} PRIVATE ${VALGRIND_INCLUDE_DIR})
        target_compile_definitions(${TARGET_NAME} PRIVATE BOOST_MYSQL_VALGRIND_TESTS)
    endif()
    
    # Coverage
    if (BOOST_MYSQL_COVERAGE)
        target_compile_options(${TARGET_NAME} PRIVATE --coverage)
        target_link_options(${TARGET_NAME} PRIVATE --coverage)
    endif()
endfunction()

# Adds a test fixture that consists of running a SQL file in the MySQL server
function (add_sql_setup_fixture)
    set(options "")
    set(oneValueArgs TEST_NAME FIXTURE_NAME SQL_FILE SKIP_VAR)
    set(multiValueArgs "")
    cmake_parse_arguments(
        SQLFIXT
        "${options}" 
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )
    
    # If this env var is defined we will skip setup 
    # (just for development) (done in the Python script)
    if (SQLFIXT_SKIP_VAR)
        set(ADDITIONAL_OPTIONS -s ${SQLFIXT_SKIP_VAR})
    endif()
    
    # Actual test
    add_test(
        NAME ${SQLFIXT_TEST_NAME}
        COMMAND
            ${Python3_EXECUTABLE} 
            ${CMAKE_SOURCE_DIR}/tools/run_sql.py 
            ${SQLFIXT_SQL_FILE}
            ${ADDITIONAL_OPTIONS}
    )
    set_tests_properties(${SQLFIXT_TEST_NAME} PROPERTIES FIXTURES_SETUP ${SQLFIXT_FIXTURE_NAME})
endfunction()

# Valgrind stuff
if (BOOST_MYSQL_VALGRIND_TESTS)
    # Locate executable
    find_program(VALGRIND_EXECUTABLE valgrind)
    if (NOT VALGRIND_EXECUTABLE)
        message(FATAL_ERROR "Cannot locate valgrind executable")
    endif()
    
    # Locate includes
    find_path(VALGRIND_INCLUDE_DIR "valgrind/memcheck.h")
    if (NOT VALGRIND_INCLUDE_DIR)
        message(FATAL_ERROR "Cannot locate valgrind include files")
    endif()

    # Helper to define tests
    function(add_memcheck_test)
        set(options "")
        set(oneValueArgs NAME)
        set(multiValueArgs COMMAND)
        cmake_parse_arguments(
            AddMemcheckTest
            "${options}" 
            "${oneValueArgs}"
            "${multiValueArgs}"
            ${ARGN}
        )
        
        add_test(
            NAME ${AddMemcheckTest_NAME}
            COMMAND 
                ${VALGRIND_EXECUTABLE}
                --leak-check=full 
                --error-limit=yes
                --suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions.txt 
                --error-exitcode=1
                --gen-suppressions=all
                ${AddMemcheckTest_COMMAND}
        )
    endfunction()
endif()

