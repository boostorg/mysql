
# GTest/GMock
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.10.0
)

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

# Unit testing. We do not use gtest_discover_tests because
# of the runtime penalty (specially considerable in integration tests)
add_executable(
	mysql_unittests
	unit/serialization.cpp
	unit/capabilities.cpp
	unit/auth.cpp
	unit/metadata.cpp
	unit/text_deserialization.cpp
	unit/binary_deserialization.cpp
	unit/value.cpp
	unit/row.cpp
	unit/error.cpp
	unit/null_bitmap.cpp
	unit/prepared_statement.cpp
)
# A codegen issue in MSVC C++17 makes gmock expectations not work
if (NOT MSVC)
	target_sources(mysql_unittests PRIVATE unit/channel.cpp)
endif()

target_include_directories(
	mysql_unittests
	PRIVATE
	common
)
target_link_libraries(
	mysql_unittests
	PRIVATE
	gtest
	gtest_main
	gmock
	mysql_asio
)
_mysql_set_target_warnings(mysql_unittests)
add_test(
	NAME mysql_unittests
	COMMAND ${CMAKE_CURRENT_BINARY_DIR}/mysql_unittests
)

# Integration testing
add_executable(
	mysql_integrationtests
	integration/metadata_validator.cpp
	integration/handshake.cpp
	integration/query.cpp
	integration/query_types.cpp
)
target_link_libraries(
	mysql_integrationtests
	PRIVATE
	gtest
	gtest_main
	gmock
	mysql_asio
)
target_include_directories(
	mysql_integrationtests
	PRIVATE
	common
)
_mysql_set_target_warnings(mysql_integrationtests)
add_test(
	NAME mysql_integrationtests
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/integration/run_tests.${_MYSQL_SHELL_EXT}
)