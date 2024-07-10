//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_NETWORK_SAMPLES_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_INCLUDE_TEST_INTEGRATION_NETWORK_SAMPLES_HPP

#include <boost/mysql/string_view.hpp>

#include <boost/test/data/monomorphic/fwd.hpp>
#include <boost/test/data/size.hpp>

#include <functional>
#include <type_traits>
#include <vector>

#include "test_integration/er_network_variant.hpp"

namespace boost {
namespace mysql {
namespace test {

class network_samples
{
    using col_t = std::vector<std::reference_wrapper<er_network_variant>>;
    using factory_t = std::function<col_t()>;

    mutable bool created_{false};
    mutable col_t data_;
    factory_t fn_;

    col_t& get() const
    {
        if (!created_)
        {
            data_ = fn_();
            created_ = true;
        }
        return data_;
    }

public:
    static constexpr int arity = 1;
    using iterator = col_t::const_iterator;

    network_samples(factory_t fn) : fn_(std::move(fn)) {}
    network_samples(std::vector<string_view> names)
        : network_samples([names]() { return get_network_variants(names); })
    {
    }

    static network_samples all() { return network_samples(&all_variants); }
    static network_samples all_with_handshake() { return network_samples(&all_variants_with_handshake); }

    boost::unit_test::data::size_t size() const { return get().size(); }
    iterator begin() const { return get().begin(); }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

namespace boost {
namespace unit_test {
namespace data {
namespace monomorphic {

template <>
struct is_dataset<mysql::test::network_samples> : std::true_type
{
};

}  // namespace monomorphic
}  // namespace data
}  // namespace unit_test
}  // namespace boost

#endif
