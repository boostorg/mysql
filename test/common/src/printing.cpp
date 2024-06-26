//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
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
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/pipeline.hpp>
#include <boost/mysql/row.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/ssl_mode.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>

#include <cstring>
#include <ostream>

#include "test_common/printing.hpp"

using namespace boost::mysql;

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
