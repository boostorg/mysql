// header

#include "protocol/binary_serialization.hpp"
#include "protocol/constants.hpp"

using boost::mysql::date;
using boost::mysql::datetime;
using boost::mysql::error_code;
using boost::mysql::string_view;
using boost::mysql::detail::serialization_context;

// Binary serialization
template <class T>
static void serialize_binary_float(serialization_context& ctx, T input)
{
    boost::endian::endian_store<T, sizeof(T), boost::endian::order::little>(ctx.first(), input);
    ctx.advance(sizeof(T));
}

static void serialize_binary_date(serialization_context& ctx, const date& input)
{
    using namespace boost::mysql::detail::binc;
    serialize(ctx, static_cast<std::uint8_t>(date_sz), input.year(), input.month(), input.day());
}

static void serialize_binary_datetime(serialization_context& ctx, const datetime& input)
{
    using namespace boost::mysql::detail::binc;

    // Serialize
    serialize(
        ctx,
        static_cast<std::uint8_t>(datetime_dhmsu_sz),
        input.year(),
        input.month(),
        input.day(),
        input.hour(),
        input.minute(),
        input.second(),
        input.microsecond()
    );
}

static void serialize_binary_time(serialization_context& ctx, const boost::mysql::time& input)
{
    using namespace boost::mysql::detail::binc;
    using namespace std::chrono;

    // Break time
    auto num_micros = duration_cast<microseconds>(input % seconds(1));
    auto num_secs = duration_cast<seconds>(input % minutes(1) - num_micros);
    auto num_mins = duration_cast<minutes>(input % hours(1) - num_secs);
    auto num_hours = duration_cast<hours>(input % days(1) - num_mins);
    auto num_days = duration_cast<days>(input - num_hours);
    std::uint8_t is_negative = (input.count() < 0) ? 1 : 0;

    // Serialize
    serialize(
        ctx,
        static_cast<std::uint8_t>(time_dhmsu_sz),
        is_negative,
        static_cast<std::uint32_t>(std::abs(num_days.count())),
        static_cast<std::uint8_t>(std::abs(num_hours.count())),
        static_cast<std::uint8_t>(std::abs(num_mins.count())),
        static_cast<std::uint8_t>(std::abs(num_secs.count())),
        static_cast<std::uint32_t>(std::abs(num_micros.count()))
    );
}

// Helper for blobs
static boost::mysql::detail::string_lenenc to_string_lenenc(boost::mysql::blob_view v)
{
    return boost::mysql::detail::string_lenenc{
        string_view(reinterpret_cast<const char*>(v.data()), v.size())};
}

std::size_t boost::mysql::detail::get_size(const serialization_context& ctx, field_view input) noexcept
{
    switch (input.kind())
    {
    case field_kind::null: return 0;
    case field_kind::int64: return 8;
    case field_kind::uint64: return 8;
    case field_kind::string: return get_size(ctx, string_lenenc(input.get_string()));
    case field_kind::blob: return get_size(ctx, to_string_lenenc(input.get_blob()));
    case field_kind::float_: return 4;
    case field_kind::double_: return 8;
    case field_kind::date: return binc::date_sz + binc::length_sz;
    case field_kind::datetime: return binc::datetime_dhmsu_sz + binc::length_sz;
    case field_kind::time: return binc::time_dhmsu_sz + binc::length_sz;
    default: BOOST_ASSERT(false); return 0;
    }
}

void boost::mysql::detail::serialize(serialization_context& ctx, field_view input) noexcept
{
    switch (input.kind())
    {
    case field_kind::null: break;
    case field_kind::int64: serialize(ctx, input.get_int64()); break;
    case field_kind::uint64: serialize(ctx, input.get_uint64()); break;
    case field_kind::string: serialize(ctx, string_lenenc(input.get_string())); break;
    case field_kind::blob: serialize(ctx, to_string_lenenc(input.get_blob())); break;
    case field_kind::float_: serialize_binary_float(ctx, input.get_float()); break;
    case field_kind::double_: serialize_binary_float(ctx, input.get_double()); break;
    case field_kind::date: serialize_binary_date(ctx, input.get_date()); break;
    case field_kind::datetime: serialize_binary_datetime(ctx, input.get_datetime()); break;
    case field_kind::time: serialize_binary_time(ctx, input.get_time()); break;
    default: BOOST_ASSERT(false); break;
    }
}