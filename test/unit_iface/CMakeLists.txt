#
# Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# Unit tests for user-visible functions and classes. Having them
# separate helps us test that we can cross the compiled library boundaries successfully
add_executable(
    boost_mysql_unitifacetests
    test/detail/datetime.cpp
    test/detail/row_impl.cpp
    test/detail/rows_iterator.cpp
    test/detail/execution_concepts.cpp
    test/detail/writable_field_traits.cpp
    test/detail/typing/meta_check_context.cpp
    test/detail/typing/pos_map.cpp
    test/detail/typing/readable_field_traits.cpp
    test/detail/typing/row_traits.cpp
    test/detail/typing/get_type_index.cpp
    test/client_errc.cpp
    test/common_server_errc.cpp
    test/mysql_server_errc.cpp
    test/mariadb_server_errc.cpp
    test/connection.cpp
    test/date.cpp
    test/datetime.cpp
    test/field_view.cpp
    test/field.cpp
    test/row_view.cpp
    test/row.cpp
    test/rows_view.cpp
    test/rows.cpp
    test/metadata.cpp
    test/diagnostics.cpp
    # statement.cpp
    # test/execution_state.cpp
    # test/static_execution_state.cpp
    # test/results.cpp
    # test/static_results.cpp
    # test/resultset_view.cpp
    # test/resultset.cpp
    # throw_on_error.cpp
    entry_point.cpp
)
target_include_directories(
    boost_mysql_unitifacetests
    PRIVATE
    "include"
)
target_link_libraries(
    boost_mysql_unitifacetests
    PRIVATE
    boost_mysql_testing
    boost_mysql
)
# if (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.16)
#     target_precompile_headers(
#         boost_mysql_unitifacetests
#         PRIVATE
#         pch.hpp
#     )
# endif()
set_source_files_properties(
    entry_point.cpp
    PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)
common_target_settings(boost_mysql_unitifacetests)

if (BOOST_MYSQL_VALGRIND_TESTS)
    add_memcheck_test(
        NAME boost_mysql_unitifacetests_memcheck
        TARGET boost_mysql_unitifacetests
    )
else()
    add_test(
        NAME boost_mysql_unitifacetests
        COMMAND boost_mysql_unitifacetests
    )
    add_dependencies(tests boost_mysql_unitifacetests)
endif()
