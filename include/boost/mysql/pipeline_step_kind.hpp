//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_PIPELINE_STEP_KIND_HPP
#define BOOST_MYSQL_PIPELINE_STEP_KIND_HPP

namespace boost {
namespace mysql {

// TODO: document
enum class pipeline_step_kind
{
    execute,
    prepare_statement,
    close_statement,
    reset_connection,
    set_character_set,
    ping,
};

}  // namespace mysql
}  // namespace boost

#endif
