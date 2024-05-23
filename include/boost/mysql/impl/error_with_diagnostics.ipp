//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_ERROR_WITH_DIAGNOSTICS_IPP
#define BOOST_MYSQL_IMPL_ERROR_WITH_DIAGNOSTICS_IPP

#pragma once

#include <boost/mysql/error_with_diagnostics.hpp>

#include <boost/throw_exception.hpp>

void boost::mysql::throw_exception_from_error(const errcode_with_diagnostics& e, const source_location& loc)
{
    ::boost::throw_exception(error_with_diagnostics(e.code, e.diag), loc);
}

#endif
