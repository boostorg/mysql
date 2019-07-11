cmake_minimum_required(VERSION 3.1.0)
set(CMAKE_CXX_STANDARD 17)
project(mysql-asio)

set(BOOST_ROOT /opt/boost_1_70_0 CACHE STRING "Path to boost installation")
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Library
add_library(
	mysql_asio
	SHARED
	src/basic_serialization.cpp
	src/message_serialization.cpp
	src/mysql_stream.cpp
	src/auth.cpp
	src/prepared_statement.cpp
)
target_link_libraries(
	mysql_asio
	PUBLIC
	Boost::system
	Threads::Threads
	OpenSSL::Crypto
)
target_include_directories(
	mysql_asio
	PUBLIC
	include
)

# Main
add_executable(
	main
	main.cpp
)
target_link_libraries(
	main
	PRIVATE
	mysql_asio
)

# Unit testing
set(GTEST_ROOT /opt/gtest)
include(GoogleTest)
find_package(GTest REQUIRED)
enable_testing()
add_executable(
	unittests
	test/basic_serialization.cpp
)
target_link_libraries(
	unittests
	PUBLIC
	GTest::GTest
	GTest::Main
	mysql_asio
)
gtest_discover_tests(unittests)
