//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "serialization_test_common.hpp"
#include "boost/mysql/detail/protocol/query_messages.hpp"

using namespace boost::mysql::test;
using namespace boost::mysql::detail;

namespace
{

INSTANTIATE_TEST_SUITE_P(ComQuery, SerializeTest, testing::Values(
	serialization_testcase(com_query_packet{
		string_eof("show databases")
	}, {
		0x03, 0x73, 0x68, 0x6f, 0x77, 0x20, 0x64, 0x61,
		0x74, 0x61, 0x62, 0x61, 0x73, 0x65, 0x73
	}, "regular")
), test_name_generator);

}
