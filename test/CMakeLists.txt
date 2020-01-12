
# Unit testing
# TODO: build tests just when required
# TODO: fetch gtest and gmock
# TODO: is there a way of adding tests to ctest (like gtest_discover_tests)
#       without runtime going nuts?
find_package(GTest REQUIRED)
find_library(GMOCK_LIBRARY gmock PATHS ${GTEST_ROOT}/lib) 
add_executable(
	unittests
	serialization.cpp
	channel.cpp
	capabilities.cpp
	auth.cpp
	metadata.cpp
	deserialize_row.cpp
	value.cpp
	row.cpp
)
target_include_directories(
	unittests
	PRIVATE
	common
)
target_link_libraries(
	unittests
	PRIVATE
	GTest::GTest
	GTest::Main
	${GMOCK_LIBRARY}
	mysql_asio
)
_mysql_set_target_warnings(unittests)
add_test(
	NAME mysql_unittests
	COMMAND ${CMAKE_CURRENT_BINARY_DIR}/unittests
)

# Integration testing
add_executable(
	integrationtests
	integration/metadata_validator.cpp
	integration/handshake.cpp
	integration/query.cpp
	integration/query_types.cpp
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
	common
)
_mysql_set_target_warnings(integrationtests)
add_test(
	NAME mysql_integrationtests
	COMMAND ${CMAKE_CURRENT_BINARY_DIR}/integrationtests
)