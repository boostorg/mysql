//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TYPED_HPP
#define BOOST_MYSQL_TYPED_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/column_type.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_kind.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>

#include <boost/describe/members.hpp>
#include <boost/describe/modifiers.hpp>
#include <boost/mp11/detail/mp_list.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/tuple.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <type_traits>

namespace boost {
namespace mysql {
namespace detail {

class meta_check_context
{
    std::ostringstream ss_;  // TODO: is default-constructing this cheap? (spoiler: no)
    std::size_t current_index_{};
    const metadata* meta_{};
    bool nullability_checked_{false};

public:
    meta_check_context() = default;
    meta_check_context(const metadata* meta) : meta_(meta) {}
    const metadata& current_meta() const noexcept { return meta_[current_index_]; }
    void advance() noexcept { ++current_index_; }
    std::size_t current_index() const noexcept { return current_index_; }
    bool nullability_checked() const noexcept { return nullability_checked_; }
    void set_nullability_checked() noexcept { nullability_checked_ = true; }
    std::ostringstream& error_stream() noexcept { return ss_; }
    std::string errors() const { return ss_.str(); }
};

// Field types
template <typename T>
struct field_traits;

template <>
struct field_traits<std::uint8_t>
{
    static void meta_check(meta_check_context& ctx)
    {
        if (ctx.current_meta().type() != column_type::tinyint)
            ctx.error_stream() << "Type <TODO: field_kind to string> is incompatible with std::uint8_t";
    }
    static error_code parse(field_view input, std::uint8_t& output)
    {
        auto v = input.get_uint64();
        assert(v < std::numeric_limits<std::uint8_t>::max());
        output = static_cast<std::uint8_t>(v);
        return error_code();
    }
    field_view serialize(std::uint8_t input) { return field_view(input); }
};

template <>
struct field_traits<std::int32_t>
{
    static void meta_check(meta_check_context& ctx)
    {
        auto t = ctx.current_meta().type();
        if (t != column_type::tinyint && t != column_type::smallint && t != column_type::mediumint &&
            (t != column_type::int_ || ctx.current_meta().is_unsigned()) && t != column_type::year)
        {
            ctx.error_stream() << "Type <TODO: column_type to string> incompatible with std::uint8_t";
        }
    }
    static error_code parse(field_view input, std::int32_t& output)
    {
        auto kind = input.kind();
        assert(kind == field_kind::int64 || kind == field_kind::uint64);
        if (kind == field_kind::int64)
        {
            auto v = input.get_int64();
            assert(v >= std::numeric_limits<std::int32_t>::min());
            assert(v <= std::numeric_limits<std::int32_t>::max());
            output = static_cast<std::int32_t>(v);
        }
        else
        {
            auto v = input.get_uint64();
            assert(v <= std::numeric_limits<std::int32_t>::max());
            output = static_cast<std::int32_t>(v);
        }
        return error_code();
    }
    field_view serialize(std::uint8_t input) { return field_view(input); }
};

// Row types
template <class RowType, bool is_describe_struct = boost::describe::has_describe_members<RowType>::value>
struct row_traits;

template <class RowType>
struct row_traits<RowType, true>
{
    using row_members = boost::describe::
        describe_members<RowType, boost::describe::mod_public | boost::describe::mod_inherited>;

    static constexpr std::size_t size = boost::mp11::mp_size<row_members>::value;

    // TODO: these mp_for_each should probably be struct functions
    static void meta_check(meta_check_context& ctx)
    {
        boost::mp11::mp_for_each<row_members>([&](auto D) {
            using FieldType = decltype(std::declval<RowType>().*D.pointer);
            field_traits<FieldType>::meta_check(ctx);
            // TODO: several failures will be reported in a single line
            if (!ctx.nullability_checked() && !ctx.current_meta().is_not_null())
            {
                ctx.error_stream(
                ) << "<TODO: report types and field names> can be NULL but the selected type "
                     "doesn't allow for it - use std::optional";
            }
            ctx.advance();
        });
    }

    static error_code parse(const field_view* from, RowType& to)
    {
        error_code ec;
        boost::mp11::mp_for_each<row_members>([&](auto D) {
            using FieldType = decltype(std::declval<RowType>().*D.pointer);
            auto code = field_traits<FieldType>::parse(*from++, to.*D.pointer);
            if (!ec)
                ec = code;
        });
        return ec;
    }
};

template <typename... FieldType>
struct row_traits<std::tuple<FieldType...>, false>
{
    using field_types = boost::mp11::mp_list<FieldType...>;
    using tuple_type = std::tuple<FieldType...>;

    static constexpr std::size_t size = std::tuple_size<tuple_type>::value;

    // TODO: these mp_for_each should probably be struct functions
    static void meta_check(meta_check_context& ctx)
    {
        boost::mp11::mp_for_each<field_types>([&](auto t) {
            using T = decltype(t);
            field_traits<T>::meta_check(ctx);
            // TODO: several failures will be reported in a single line
            if (!ctx.nullability_checked() && !ctx.current_meta().is_not_null())
            {
                ctx.error_stream(
                ) << "<TODO: report types and field names> can be NULL but the selected type "
                     "doesn't allow for it - use std::optional";
            }
            ctx.advance();
        });
    }

    static error_code parse(const field_view* from, tuple_type& to)
    {
        error_code ec;
        boost::mp11::tuple_for_each(to, [&](auto& f) {
            using T = typename std::remove_reference<decltype(f)>::type;
            auto code = field_traits<T>::parse(*from++, f);
            if (!ec)
                ec = code;
        });
        return ec;
    }
};

template <class RowType>
error_code do_meta_check(metadata_collection_view meta, diagnostics& diag)
{
    meta_check_context check_ctx(meta.data());
    constexpr std::size_t type_size = row_traits<RowType>::size;
    if (type_size == meta.size())
    {
        row_traits<RowType>::meta_check(check_ctx);
    }
    else
    {
        check_ctx.error_stream() << "Type is incompatible with query: the provided type has " << type_size
                                 << " members, while the query returned " << meta.size() << " items";
    }

    auto errors = check_ctx.errors();
    if (!errors.empty())
    {
        diagnostics_access::assign(diag, errors, false);

        return client_errc::type_mismatch;
    }
    return error_code();
}

template <class RowType>
error_code do_parse(span<const field_view, row_traits<RowType>::size> from, RowType& to)
{
    row_traits<RowType>::parse(from.data(), to);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
