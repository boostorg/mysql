
# Perform the search
find_program(Mysqlvalgrind_EXECUTABLE valgrind)
find_path(Mysqlvalgrind_INCLUDE_DIR "valgrind/memcheck.h")

# Inform CMake of the results
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Mysqlvalgrind
  FOUND_VAR Mysqlvalgrind_FOUND
  REQUIRED_VARS
    Mysqlvalgrind_EXECUTABLE
    Mysqlvalgrind_INCLUDE_DIR
)

# Valgrind includes
if(Mysqlvalgrind_FOUND AND NOT TARGET Mysqlvalgrind::Mysqlvalgrind)
  add_library(Mysqlvalgrind::Mysqlvalgrind UNKNOWN IMPORTED)
  set_target_properties(Mysqlvalgrind::Mysqlvalgrind PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${Mysqlvalgrind_INCLUDE_DIR}"
  )
endif()

if (Mysqlvalgrind_FOUND AND NOT COMMAND MysqlValgrind_AddTest)
	function(Mysqlvalgrind_AddMemcheckTest)
		set(options "")
    	set(oneValueArgs NAME)
    	set(multiValueArgs COMMAND)
    	cmake_parse_arguments(
    		AddMemcheckTest
    		"${options}" 
    		"${oneValueArgs}"
            "${multiValueArgs}"
            ${ARGN}
        )
        
		add_test(
			NAME ${AddMemcheckTest_NAME}
			COMMAND 
				${Mysqlvalgrind_EXECUTABLE}
				--leak-check=full 
				--error-limit=yes
				--suppressions=${CMAKE_SOURCE_DIR}/test/common/valgrind_suppressions.txt 
				--error-exitcode=1
				--gen-suppressions=all
				${AddMemcheckTest_COMMAND}
		)
	endfunction() 
endif()

