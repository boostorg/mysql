cmake_minimum_required(VERSION 3.11.0)
project(mysql-asio)

include(FetchContent)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

set(CMAKE_CXX_STANDARD 17)
enable_testing()

function (_mysql_set_target_warnings TARGET_NAME)
	if(MSVC)
		target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
	else()
		target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -pedantic -Werror)
	endif()
endfunction()

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

# Library (header-only)
add_library(
    mysql_asio
    INTERFACE
)

target_link_libraries(
    mysql_asio
	INTERFACE
	Boost::system # TODO: can we specify asio/beast?
	Threads::Threads
	OpenSSL::Crypto
)
target_include_directories(
	mysql_asio
	INTERFACE
	include
	${date_SOURCE_DIR}/include
)

# Examples
add_subdirectory(examples)

# Tests
add_subdirectory(test)
