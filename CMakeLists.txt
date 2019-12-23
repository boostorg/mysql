cmake_minimum_required(VERSION 3.11.0)
project(mysql-asio)

include(FetchContent)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

set(CMAKE_CXX_STANDARD 17)

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

# Unit testing
# TODO: build tests just when required
# TODO: fetch gtest and gmock
# TODO: is there a way of adding tests to ctest (like gtest_discover_tests)
#       without runtime going nuts?
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
	test/value.cpp
	test/row.cpp
)
target_include_directories(
	unittests
	PRIVATE
	test/common
)
target_link_libraries(
	unittests
	PUBLIC
	GTest::GTest
	GTest::Main
	${GMOCK_LIBRARY}
	mysql_asio
)
add_test(
	NAME mysql_unittests
	COMMAND ${CMAKE_CURRENT_BINARY_DIR}/unittests
)

# Integration testing
add_executable(
	integrationtests
	test/integration/metadata_validator.cpp
	test/integration/handshake.cpp
	test/integration/query.cpp
	test/integration/query_types.cpp
)
target_link_libraries(
	integrationtests
	PRIVATE
	GTest::GTest
	GTest::Main
	mysql_asio
)
target_include_directories(
	integrationtests
	PRIVATE
	test/common
)
add_test(
	NAME mysql_integrationtests
	COMMAND ${CMAKE_CURRENT_BINARY_DIR}/integrationtests
)
