cmake_minimum_required(VERSION 3.1.0)
set(CMAKE_CXX_STANDARD 17)
project(mysql-asio)

set(BOOST_ROOT /opt/boost_1_70_0 CACHE STRING "Path to boost installation")
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

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
