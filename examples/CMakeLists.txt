
# Syncrhonous
add_executable(
	example_query_sync
	query_sync.cpp
)
target_link_libraries(
	example_query_sync
	PRIVATE
	mysql_asio
)

# Asynchronous
add_executable(
	example_query_async
	query_async.cpp
)
target_link_libraries(
	example_query_async
	PRIVATE
	mysql_asio
)

# Add them as automated tests
add_test(
	NAME examples
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_examples.sh
)

