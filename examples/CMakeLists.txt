
add_executable(
	example_query_sync
	query_sync.cpp
)
target_link_libraries(
	example_query_sync
	PRIVATE
	mysql_asio
)

add_test(
	NAME example_query_sync
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_examples.sh
)