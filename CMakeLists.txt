cmake_minimum_required(VERSION 3.11.0)
set(CMAKE_CXX_STANDARD 17)
project(mysql-asio)

include(FetchContent)

set(BOOST_ROOT /opt/boost_1_70_0 CACHE STRING "Path to boost installation")
find_package(Boost REQUIRED COMPONENTS system)
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
    # TODO: Building the project requires CMake 3.14
    # add_subdirectory(${date_SOURCE_DIR} ${date_BINARY_DIR})
endif()

# Library
#add_library(
#	mysql_asio
#	SHARED
#	src/basic_serialization.cpp
#	src/message_serialization.cpp
#	src/auth.cpp
#)
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
)
target_include_directories(
	mysql_asio
	INTERFACE
	include
	${date_SOURCE_DIR}/include
)

# Main
#add_executable(
#	main
#	main.cpp
#)
#target_link_libraries(
#	main
#	PRIVATE
#	mysql_asio
#)

# Unit testing
set(GTEST_ROOT /opt/gtest)
include(GoogleTest)
find_package(GTest REQUIRED)
find_library(GMOCK_LIBRARY gmock PATHS ${GTEST_ROOT}/lib) 
enable_testing()
add_executable(
	unittests
	test/serialization.cpp
	test/channel.cpp
	test/capabilities.cpp
	test/auth.cpp
	test/metadata.cpp
	test/deserialize_row.cpp
)
target_link_libraries(
	unittests
	PUBLIC
	GTest::GTest
	GTest::Main
	${GMOCK_LIBRARY}
	mysql_asio
)
gtest_discover_tests(unittests)

# Integration testing
add_executable(
	integrationtests
	test/integration/metadata_validator.cpp
	test/integration/handshake.cpp
	test/integration/query.cpp
)
target_link_libraries(
	integrationtests
	PRIVATE
	GTest::GTest
	GTest::Main
	mysql_asio
)
gtest_discover_tests(integrationtests)
