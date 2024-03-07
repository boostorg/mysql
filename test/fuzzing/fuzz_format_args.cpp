//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/blob_view.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/format_sql.hpp>
#include <boost/mysql/string_view.hpp>
#include <boost/mysql/time.hpp>

#include <boost/endian/detail/endian_load.hpp>
#include <boost/endian/detail/order.hpp>
#include <boost/variant2/variant.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

using namespace boost::mysql;

namespace {

// format_arg is designed as function argument. For identifiers,
// it only stores a reference. This type avoids lifetime problems
using owning_format_arg = boost::variant2::variant<
    std::nullptr_t,
    int64_t,
    uint64_t,
    float,
    double,
    string_view,
    blob_view,
    date,
    datetime,
    boost::mysql::time,
    identifier>;

// Helper for parsing the input sample from the binary string provided by the fuzzer
// This follows a "never fail" approach
class sample_parser
{
    const uint8_t* it_;
    const uint8_t* end_;

    std::size_t size() const noexcept { return static_cast<std::size_t>(end_ - it_); }

    template <class T>
    T get()
    {
        if (size() < sizeof(T))
            return T{};

        auto res = boost::endian::endian_load<T, sizeof(T), boost::endian::order::little>(it_);
        it_ += sizeof(T);
        return res;
    }

    blob_view get_blob()
    {
        std::size_t len = get<uint8_t>() % 128u;
        auto actual_len = (std::min)(len, size());
        blob_view res(it_, actual_len);
        it_ += actual_len;
        return res;
    }

    string_view get_string()
    {
        auto res = get_blob();
        return {reinterpret_cast<const char*>(res.data()), res.size()};
    }

    date get_date() { return date(get<uint16_t>(), get<uint8_t>(), get<uint8_t>()); }

    datetime get_datetime()
    {
        return datetime(
            get<uint16_t>(),
            get<uint8_t>(),
            get<uint8_t>(),
            get<uint8_t>(),
            get<uint8_t>(),
            get<uint8_t>(),
            get<uint32_t>()
        );
    }

    boost::mysql::time get_time() { return boost::mysql::time(get<int64_t>()); }

    owning_format_arg get_format_arg(uint8_t type)
    {
        switch (type % 13)
        {
        case 0:
        default: return nullptr;
        case 1: return get<int64_t>();
        case 2: return get<uint64_t>();
        case 3: return get<float>();
        case 4: return get<double>();
        case 5: return get_string();
        case 6: return get_blob();
        case 7: return get_date();
        case 8: return get_datetime();
        case 9: return get_time();
        case 10: return identifier(get_string());
        case 11: return identifier(get_string(), get_string());
        case 12: return identifier(get_string(), get_string(), get_string());
        }
    }

public:
    sample_parser(const uint8_t* data, size_t size) noexcept : it_(data), end_(data + size) {}

    std::pair<owning_format_arg, owning_format_arg> parse()
    {
        // Types
        uint8_t type_code = get<uint8_t>();
        uint8_t type0 = type_code & 0x0f;
        uint8_t type1 = type_code & 0xf0 >> 4;

        // Arguments
        return {get_format_arg(type0), get_format_arg(type1)};
    }
};

}  // namespace

// Converts to format_arg
static format_arg to_format_arg(const owning_format_arg& input)
{
    return boost::variant2::visit([](const auto& v) { return format_arg{"", v}; }, input);
}

static bool call_format_sql(const uint8_t* data, size_t size) noexcept
{
    // Parse the sample
    auto sample = sample_parser(data, size).parse();

    // Use a format context so we can avoid exceptions
    format_context ctx({utf8mb4_charset, true});
    format_sql_to(ctx, "{}, {}", {to_format_arg(sample.first), to_format_arg(sample.second)});

    return std::move(ctx).get().has_value();
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Note: this code should never throw exceptions, for any kind of input
    call_format_sql(data, size);
    return 0;
}
