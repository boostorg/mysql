
function (_mysql_add_example EXECUTABLE_NAME CPPFILE)
	add_executable(
		${EXECUTABLE_NAME}
		${CPPFILE}
	)
	target_link_libraries(
		${EXECUTABLE_NAME}
		PRIVATE
		mysql_asio
	)
	_mysql_set_target_warnings(${EXECUTABLE_NAME})
endfunction()

_mysql_add_example(example_query_sync query_sync.cpp)
_mysql_add_example(example_query_async query_async.cpp)
_mysql_add_example(example_metadata metadata.cpp)

# Add them as automated tests
add_test(
	NAME examples
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_examples.${_MYSQL_SHELL_EXT}
)

