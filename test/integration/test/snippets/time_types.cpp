//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/results.hpp>

#include <boost/test/unit_test.hpp>

#include <iostream>

#include "test_integration/snippets/get_connection.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

namespace {

BOOST_AUTO_TEST_CASE(section_time_types)
{
    auto& conn = get_connection();

    {
        //[time_types_date_as_time_point
        date d(2020, 2, 19);                      // d holds "2020-02-19"
        date::time_point tp = d.as_time_point();  // now use tp normally

        //]
        BOOST_TEST(date(tp) == d);
    }
    {
        //[time_types_date_valid
        date d1(2020, 2, 19);  // regular date
        bool v1 = d1.valid();  // true
        date d2(2020, 0, 19);  // invalid date
        bool v2 = d2.valid();  // false

        //]
        BOOST_TEST(v1);
        BOOST_TEST(!v2);
    }
    {
        //[time_types_date_get_time_point
        date d = /* obtain a date somehow */ date(2020, 2, 29);
        if (d.valid())
        {
            // Same as as_time_point, but doesn't check for validity
            // Caution: be sure to check for validity.
            // If d is not valid, get_time_point results in undefined behavior
            date::time_point tp = d.get_time_point();

            // Use tp as required
            std::cout << tp.time_since_epoch().count() << std::endl;
        }
        else
        {
            // the date is invalid
            std::cout << "Invalid date" << std::endl;
        }
        //]
    }
    {
        //[time_types_datetime
        datetime dt1(2020, 10, 11, 10, 20, 59, 123456);  // regular datetime 2020-10-11 10:20:59.123456
        bool v1 = dt1.valid();                           // true
        datetime dt2(2020, 0, 11, 10, 20, 59);           // invalid datetime 2020-00-10 10:20:59.000000
        bool v2 = dt2.valid();                           // false

        datetime::time_point tp = dt1.as_time_point();  // convert to time_point

        //]
        BOOST_TEST(v1);
        BOOST_TEST(!v2);
        BOOST_TEST(datetime(tp) == dt1);
    }
    {
        //[time_types_timestamp_setup
        results result;
        conn.execute(
            R"%(
                CREATE TEMPORARY TABLE events (
                    id INT PRIMARY KEY AUTO_INCREMENT,
                    t TIMESTAMP,
                    contents VARCHAR(256)
                )
            )%",
            result
        );
        //]

        //[time_types_timestamp_stmts
        auto insert_stmt = conn.prepare_statement("INSERT INTO events (t, contents) VALUES (?, ?)");
        auto select_stmt = conn.prepare_statement("SELECT id, t, contents FROM events WHERE t > ?");
        //]

        //[time_types_timestamp_set_time_zone
        // This change has session scope. All operations after this query
        // will now use UTC for TIMESTAMPs. Other sessions will not see the change.
        // If you need to reconnect the connection, you need to run this again.
        // If your MySQL server supports named time zones, you can also use
        // "SET time_zone = 'UTC'"
        conn.execute("SET time_zone = '+00:00'", result);
        //]

        //[time_types_timestamp_insert
        // Get the timestamp of the event. This may have been provided by an external system
        // For the sake of example, we will use the current timestamp
        datetime event_timestamp = datetime::now();

        // event_timestamp will be interpreted as UTC if you have run SET time_zone
        conn.execute(insert_stmt.bind(event_timestamp, "Something happened"), result);
        //]

        //[time_types_timestamp_select
        // Get the timestamp threshold from the user. We will use a constant for the sake of example
        datetime threshold = datetime(2022, 1, 1);  // get events that happened after 2022-01-01

        // threshold will be interpreted as UTC. The retrieved events will have their
        // `t` column in UTC
        conn.execute(select_stmt.bind(threshold), result);
        //]
    }
}

}  // namespace
