#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

find_package(Python3 REQUIRED)

# Adds a test fixture that consists of running a SQL file in the MySQL server
function (_mysql_sql_setup_fixture)
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