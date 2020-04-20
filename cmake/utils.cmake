#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Utility function to set warnings and other compile properties of
# our test targets
function(_mysql_common_target_settings TARGET_NAME)
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
        # For some reason, Appveyor setup needs this to link against coroutine
        target_link_directories(${TARGET_NAME} PRIVATE ${Boost_LIBRARY_DIRS})
        target_compile_options(${TARGET_NAME} PRIVATE /bigobj) # Prevent failures on Windows
    else()
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -pedantic -Werror)
    endif()
    set_target_properties(${TARGET_NAME} PROPERTIES CXX_EXTENSIONS OFF) # disable extensions
endfunction()
