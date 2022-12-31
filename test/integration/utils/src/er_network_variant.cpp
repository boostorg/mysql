//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/core/span.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <unordered_map>

#include "er_impl_common.hpp"
#include "er_network_variant.hpp"

using namespace boost::mysql::test;

static std::vector<er_network_variant*> make_all_variants()
{
    std::vector<er_network_variant*> res;
    add_sync_errc(res);
    add_sync_exc(res);
    add_async_callback(res);
    add_async_callback_noerrinfo(res);
    add_async_future(res);
    add_async_coroutine(res);
    add_async_coroutinecpp20(res);
    add_default_completion_tokens(res);
    return res;
}

static std::unordered_map<std::string, er_network_variant*> make_variants_map()
{
    std::unordered_map<std::string, er_network_variant*> res;
    for (auto* var : all_variants())
        res[var->name()] = var;
    return res;
}

boost::span<er_network_variant*> boost::mysql::test::all_variants()
{
    static auto res = make_all_variants();
    return res;
}

er_network_variant* boost::mysql::test::get_variant(string_view name)
{
    static auto by_name = make_variants_map();
    std::string name_str(name);
    auto it = by_name.find(name_str);
    if (it == by_name.end())
        throw std::out_of_range("Unknown network variant: " + name_str);
    return it->second;
}
