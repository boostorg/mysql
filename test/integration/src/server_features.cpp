//

#include <boost/mysql/string_view.hpp>

#include <boost/core/span.hpp>
#include <boost/function/function_base.hpp>
#include <boost/test/framework.hpp>
#include <boost/test/tools/assertion_result.hpp>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <vector>

#include "test_integration/server_features.hpp"

using namespace boost::mysql;

static std::vector<string_view> split_list(string_view s)
{
    std::vector<string_view> res;
    std::size_t pos = 0;
    if (!s.empty())
    {
        while ((pos = s.find(' ')) != string_view::npos)
        {
            res.push_back(s.substr(0, pos));
            s = s.substr(pos + 1);
        }
        res.push_back(s);
    }
    return res;
}

static test::server_features do_get_server_features()
{
    // Get the command line arguments
    const auto& suite = boost::unit_test::framework::master_test_suite();
    BOOST_ASSERT(suite.argc >= 1);

    // Attempt to find the parameter we want. Fail if we find unknown arguments.
    // Note that argv[0] is the program name
    const string_view prefix = "--disabled-server-features=";
    string_view disabled_features_str;
    for (int i = 1; i < suite.argc; ++i)
    {
        string_view arg = suite.argv[1];
        if (!arg.starts_with(prefix))
        {
            std::cerr << "Unknown command line argument: " << arg << '\n';
            exit(1);
        }
        disabled_features_str = arg.substr(prefix.size());
    }

    // Parse the disabled features list
    auto disabled_features = split_list(disabled_features_str);

    // The list of possible features
    const struct possible_feature_t
    {
        string_view name;
        test::server_feature_t ptr;
    } possible_features[]{
        {"unix-sockets",          &test::server_features::unix_sockets         },
        {"sha256",                &test::server_features::sha256               },
        {"json-type",             &test::server_features::json_type            },
        {"regex-error-codes",     &test::server_features::regex_error_codes    },
        {"dup-query-error-codes", &test::server_features::dup_query_error_codes},
    };

    // All features are enabled, by default
    test::server_features res;
    for (const auto feature : possible_features)
    {
        res.*feature.ptr = true;
    }

    // Match disabled features against the possible set
    for (auto feature : disabled_features)
    {
        auto it = std::find_if(
            std::begin(possible_features),
            std::end(possible_features),
            [feature](possible_feature_t v) { return v.name == feature; }
        );
        if (it == std::end(possible_features))
        {
            std::cerr << "Unknown disabled feature: " << feature << std::endl;
            exit(1);
        }
        res.*it->ptr = false;
    }

    // Report the configuration
    std::cout << "Server features:\n";
    for (auto feature : possible_features)
    {
        std::cout << "+ " << feature.name << ": " << res.*feature.ptr << '\n';
    }
    std::cout << '\n';

    // Done
    return res;
}

boost::mysql::test::server_features boost::mysql::test::get_server_features()
{
    static server_features res = do_get_server_features();
    return res;
}

boost::unit_test::precondition boost::mysql::test::run_if(server_feature_t feature)
{
    return unit_test::precondition([feature](unit_test::test_unit_id) {
        return get_server_features().*feature;
    });
}

boost::unit_test::precondition boost::mysql::test::run_if(
    server_feature_t feature1,
    server_feature_t feature2
)
{
    return unit_test::precondition([=](unit_test::test_unit_id) {
        const auto supported = get_server_features();
        return supported.*feature1 && supported.*feature2;
    });
}
