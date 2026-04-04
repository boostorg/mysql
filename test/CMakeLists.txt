#
# Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# boost_mysql_testing contains common definitions, includes and settings.
add_library(boost_mysql_testing STATIC
    common/src/utils.cpp
)
target_link_libraries(boost_mysql_testing PUBLIC
    Boost::mycosql
    Boost::core
    Boost::pfr
)
target_include_directories(boost_mysql_testing PUBLIC common/include)

# # Unit testing
# add_subdirectory(unit)

# Integration testing
if(BOOST_MYSQL_INTEGRATION_TESTS)
    add_subdirectory(integration)
endif()
