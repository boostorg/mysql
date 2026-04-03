//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/error_with_diagnostics.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>

#include <boost/assert/source_location.hpp>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/check_meta.hpp"
#include "test_common/io_context_fixture.hpp"
#include "test_common/printing.hpp"
#include "test_common/validate_string_contains.hpp"

using namespace boost::mysql;
using namespace boost::mysql::test;

//
// assert_buffer_equals.hpp
//
static std::string format_buffer(std::span<const std::uint8_t> buff)
{
    std::ostringstream os;
    os << std::setfill('0') << std::hex << "{ ";
    for (std::size_t i = 0; i < buff.size(); ++i)
    {
        os << "0x" << std::setw(2) << static_cast<int>(buff.data()[i]) << ", ";
    }
    os << "}";
    return os.str();
}

std::ostream& boost::mysql::test::operator<<(std::ostream& os, buffer_printer buff)
{
    return os << format_buffer(buff.buff);
}

bool boost::mysql::test::buffer_equals(std::span<const std::uint8_t> b1, std::span<const std::uint8_t> b2)
{
    // If any of the buffers are empty (data() == nullptr), prevent
    // calling memcmp (UB)
    if (b1.size() == 0 || b2.size() == 0)
        return b1.size() == 0 && b2.size() == 0;

    if (b1.size() != b2.size())
        return false;

    return ::std::memcmp(b1.data(), b2.data(), b1.size()) == 0;
}

//
// printing.hpp
//

std::ostream& boost::mysql::operator<<(std::ostream& os, client_errc v) { return os << error_code(v); }

std::ostream& boost::mysql::operator<<(std::ostream& os, common_server_errc v) { return os << error_code(v); }

std::ostream& boost::mysql::operator<<(std::ostream& os, const diagnostics& diag)
{
    const auto& impl = detail::access::get_impl(diag);
    return os << "diagnostics{ " << (impl.is_server ? ".server_message" : ".client_message") << " = \""
              << impl.msg << "\" }";
}

std::ostream& boost::mysql::operator<<(std::ostream& os, const row_view& value)
{
    os << '{';
    if (!value.empty())
    {
        os << value[0];
        for (auto it = std::next(value.begin()); it != value.end(); ++it)
        {
            os << ", " << *it;
        }
    }
    return os << '}';
}

std::ostream& boost::mysql::operator<<(std::ostream& os, const row& r) { return os << row_view(r); }

static const char* to_string(metadata_mode v)
{
    switch (v)
    {
    case metadata_mode::full: return "metadata_mode::full";
    case metadata_mode::minimal: return "metadata_mode::minimal";
    default: return "<unknown metadata_mode>";
    }
}

std::ostream& boost::mysql::operator<<(std::ostream& os, metadata_mode v) { return os << ::to_string(v); }

static const char* to_string(ssl_mode v)
{
    switch (v)
    {
    case ssl_mode::disable: return "ssl_mode::disable";
    case ssl_mode::enable: return "ssl_mode::enable";
    case ssl_mode::require: return "ssl_mode::require";
    default: return "<unknown ssl_mode>";
    }
}

std::ostream& boost::mysql::operator<<(std::ostream& os, ssl_mode v) { return os << ::to_string(v); }

// character set
bool boost::mysql::operator==(const character_set& lhs, const character_set& rhs)
{
    if (lhs.name == nullptr || rhs.name == nullptr)
        return lhs.name == rhs.name;
    return std::strcmp(lhs.name, rhs.name) == 0 && lhs.next_char == rhs.next_char;
}

std::ostream& boost::mysql::operator<<(std::ostream& os, const character_set& v)
{
    if (v.name == nullptr)
        return os << "character_set()";
    else
        return os << "character_set(\"" << v.name << "\", .next_char? = " << static_cast<bool>(v.next_char)
                  << ")";
}

//
// check_meta.hpp
//
void boost::mysql::test::check_meta(metadata_collection_view meta, const std::vector<column_type>& expected_types)
{
    auto types = meta | std::ranges::views::transform([](const metadata& m) { return m.type(); });
    BOOST_TEST_ALL_EQ(types.begin(), types.end(), expected_types.begin(), expected_types.end());
}

void boost::mysql::test::check_meta(
    metadata_collection_view meta,
    const std::vector<std::pair<column_type, std::string_view>>& expected
)
{
    auto types = meta | std::ranges::views::transform([](const metadata& m) { return m.type(); });
    auto expected_types = expected |
                          std::ranges::views::transform(
                              [](const std::pair<column_type, std::string_view>& p) { return p.first; }
                          );
    BOOST_TEST_ALL_EQ(types.begin(), types.end(), expected_types.begin(), expected_types.end());

    auto names = meta | std::ranges::views::transform([](const metadata& m) { return m.column_name(); });
    auto expected_names = expected |
                          std::ranges::views::transform(
                              [](const std::pair<column_type, std::string_view>& p) { return p.second; }
                          );
    BOOST_TEST_ALL_EQ(names.begin(), names.end(), expected_names.begin(), expected_names.end());
}

//
// validate_string_contains.hpp
//
void boost::mysql::test::validate_string_contains(
    std::string value,
    const std::vector<std::string>& to_check,
    boost::source_location loc
)
{
    std::transform(value.begin(), value.end(), value.begin(), [](char c) {
        return static_cast<char>(tolower(c));
    });
    for (const auto& elm : to_check)
    {
        if (!BOOST_TEST(value.find(elm) != std::string::npos))
        {
            std::cerr << "   Substring '" << elm << "' not found in '" << value << "'\n"
                      << "   Called from " << loc << "\n";
        }
    }
}

//
// io_context_fixture.hpp
//
boost::mysql::test::io_context_fixture::~io_context_fixture()
{
    // Verify that our tests don't leave unfinished work
    ctx.poll();
    BOOST_TEST(ctx.stopped());
}
