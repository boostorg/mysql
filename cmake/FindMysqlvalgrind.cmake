#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Perform the search
find_program(Mysqlvalgrind_EXECUTABLE valgrind)
find_path(Mysqlvalgrind_INCLUDE_DIR "valgrind/memcheck.h")

# Inform CMake of the results
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Mysqlvalgrind
    DEFAULT_MSG
    Mysqlvalgrind_EXECUTABLE
    Mysqlvalgrind_INCLUDE_DIR
)

# Valgrind includes
if(Mysqlvalgrind_FOUND AND NOT TARGET Mysqlvalgrind::Mysqlvalgrind)
    add_library(Mysqlvalgrind::Mysqlvalgrind INTERFACE IMPORTED)
    target_include_directories(Mysqlvalgrind::Mysqlvalgrind INTERFACE ${Mysqlvalgrind_INCLUDE_DIR})
    target_compile_definitions(Mysqlvalgrind::Mysqlvalgrind INTERFACE BOOST_MYSQL_VALGRIND_TESTS)
endif()

if (Mysqlvalgrind_FOUND AND NOT COMMAND MysqlValgrind_AddTest)
    function(Mysqlvalgrind_AddMemcheckTest)
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
                ${Mysqlvalgrind_EXECUTABLE}
                --leak-check=full 
                --error-limit=yes
                --suppressions=${CMAKE_SOURCE_DIR}/tools/valgrind_suppressions.txt 
                --error-exitcode=1
                --gen-suppressions=all
                ${AddMemcheckTest_COMMAND}
        )
    endfunction() 
endif()

