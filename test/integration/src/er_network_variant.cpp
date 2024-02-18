//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "test_integration/er_network_variant.hpp"

#include <exception>
#include <unordered_map>

#include "er_impl_common.hpp"

using namespace boost::mysql::test;
using boost::mysql::error_code;

static std::vector<er_network_variant*> make_all_variants()
{
    std::vector<er_network_variant*> res;
    add_sync_errc(res);
    add_sync_exc(res);
    add_async_callback(res);
    add_async_coroutines(res);
    add_async_coroutinescpp20(res);
    return res;
}

static std::unordered_map<std::string, er_network_variant*> make_variants_map()
{
    std::unordered_map<std::string, er_network_variant*> res;
    for (auto* var : all_variants())
        res[var->name()] = var;
    return res;
}

static std::vector<er_network_variant*> make_all_variants_with_handshake()
{
    std::vector<er_network_variant*> res;
    for (auto* var : all_variants())
    {
        if (var->supports_handshake())
            res.push_back(var);
    }
    return res;
}

boost::span<er_network_variant*> boost::mysql::test::all_variants()
{
    static auto res = make_all_variants();
    return res;
}

boost::span<er_network_variant*> boost::mysql::test::all_variants_with_handshake()
{
    static auto res = make_all_variants_with_handshake();
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
