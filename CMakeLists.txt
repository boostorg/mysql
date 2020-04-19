cmake_minimum_required(VERSION 3.13.0) # Determined by requiring Boost 1.70
project(mysql-asio)

# We require having access to cmake/ in the module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# If we are the top-level project, enable testing
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
    	set(_MYSQL_TESTING_ENABLED ON)
    endif()
endif()

option(BOOST_MYSQL_VALGRIND_TESTS OFF "Whether to run Valgrind tests or not (requires Valgrind)")
mark_as_advanced(BOOST_MYSQL_VALGRIND_TESTS)

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

set(_MYSQL_SHELL_EXT $<IF:$<PLATFORM_ID:Windows>,bat,sh>)

# Dependencies
include(FetchContent)

find_package(Boost 1.70.0 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Date
FetchContent_Declare(
    date
    GIT_REPOSITORY https://github.com/HowardHinnant/date.git
    GIT_TAG        v2.4.1
)

FetchContent_GetProperties(date)
if(NOT date_POPULATED)
    FetchContent_Populate(date)
    # Building date requires CMake 3.14, and we only need headers
endif()

# Interface library (header-only)
add_library(
    mysql_asio
    INTERFACE
)

target_link_libraries(
    mysql_asio
	INTERFACE
	Boost::system
	Threads::Threads
	OpenSSL::Crypto
	OpenSSL::SSL
)
target_include_directories(
	mysql_asio
	INTERFACE
	include
	${date_SOURCE_DIR}/include
)
target_compile_features(
	mysql_asio
	INTERFACE
	cxx_std_17
)

# Examples
if(_MYSQL_TESTING_ENABLED)
	add_subdirectory(example)
endif()

# Tests
if(_MYSQL_TESTING_ENABLED)
	add_subdirectory(test)
endif()
