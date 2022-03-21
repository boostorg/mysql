//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_AUTH_AUTH_CALCULATOR_HPP
#define BOOST_MYSQL_DETAIL_AUTH_AUTH_CALCULATOR_HPP

#include <boost/mysql/error.hpp>
#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <array>
#include <vector>
#include <boost/utility/string_view.hpp>

namespace boost {
namespace mysql {
namespace detail {

struct authentication_plugin
{
    using calculator_signature = error_code (*)(
        boost::string_view password,
        boost::string_view challenge,
        bool use_ssl,
        bytestring& output
    );

    boost::string_view name;
    calculator_signature calculator;
};

class auth_calculator
{
    const authentication_plugin* plugin_ {nullptr};
    bytestring response_;

    inline static const authentication_plugin* find_plugin(boost::string_view name);
public:
    inline error_code calculate(
        boost::string_view plugin_name,
        boost::string_view password,
        boost::string_view challenge,
        bool use_ssl
    );
    boost::string_view response() const noexcept
    {
        return boost::string_view(reinterpret_cast<const char*>(response_.data()), response_.size());
    }
    boost::string_view plugin_name() const noexcept
    {
        assert(plugin_);
        return plugin_->name;
    }
};

} // detail
} // mysql
} // boost

#include <boost/mysql/detail/auth/impl/auth_calculator.ipp>

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUTH_AUTH_CALCULATOR_HPP_ */
