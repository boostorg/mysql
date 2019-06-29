cmake_minimum_required(VERSION 3.1.0)
set(CMAKE_CXX_STANDARD 17)
project(mysql-asio)

set(BOOST_ROOT /opt/boost_1_70_0 CACHE STRING "Path to boost installation")
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

add_executable(
	main
	main.cpp
)

target_link_libraries(
	main
	PRIVATE
	Boost::system
	Threads::Threads
)

target_include_directories(
	main
	PUBLIC
	include
)

# testing
set(GTEST_ROOT /opt/gtest)
find_package(GTest REQUIRED)
enable_testing()
include(GoogleTest)
add_executable(
	unittests
	test/deserialization.cpp
)
target_link_libraries(
	unittests
	PUBLIC
	GTest::GTest
	GTest::Main
)
target_include_directories(
	unittests
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)
