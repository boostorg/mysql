//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXECUTE_OPTIONS_HPP
#define BOOST_MYSQL_EXECUTE_OPTIONS_HPP

#include <boost/mysql/detail/auxiliar/field_type_traits.hpp>

#include <iterator>
#include <type_traits>

namespace boost {
namespace mysql {

/**
 * \brief Represents the parameters required to execute a [reflink statement_base].
 * \details In essence, this class contains an iterator range \\[first, last), pointing
 * to a sequence of [reflink field_view]s that will be used as parameters to execute a
 * [reflink statement_base]. FieldViewFwdIterator must meet the [reflink FieldViewFwdIterator]
 * type requirements.
 *
 * In the future, this class may define extra members providing finer control
 * over prepared statement execution.
 *
 * The [refmem execute_params make_execute_params] helper functions make it easier to create
 * instances of this class.
 */
class execute_options
{
public:
};

}  // namespace mysql
}  // namespace boost

#endif
