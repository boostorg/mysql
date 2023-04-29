//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPING_ROW_TRAITS_HPP
#define BOOST_MYSQL_DETAIL_TYPING_ROW_TRAITS_HPP

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/typing/meta_check_context.hpp>
#include <boost/mysql/detail/typing/readable_field_traits.hpp>
#include <boost/mysql/impl/diagnostics.hpp>

#include <boost/describe/members.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/detail/mp_list.hpp>
#include <boost/mp11/utility.hpp>

#include <cstddef>
#include <tuple>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

// TODO: this requires C++14, assert it somehow

// Helpers to check that all the fields satisfy ReadableField
// and produce meaningful error messages, with the offending field type, at least
template <class TypeList>
static constexpr bool check_readable_field() noexcept
{
    mp11::mp_for_each<TypeList>([](auto type_identity) {
        using T = typename decltype(type_identity)::type;
        static_assert(
            is_readable_field<T>::value,
            "You're trying to use an unsupported field type in a row type. Review your row type definitions."
        );
    });
    return true;
}

// Workaround std::array::data not being constexpr in C++14
template <class T, std::size_t N>
struct array_wrapper
{
    T data[N];
};

// Helpers
class parse_functor
{
    const field_view* fields_;
    const std::size_t* pos_map_;
    error_code ec_;

public:
    parse_functor(const field_view* fields, const std::size_t* pos_map) noexcept
        : fields_(fields), pos_map_(pos_map)
    {
    }

    template <class ReadableField>
    void operator()(ReadableField& output)
    {
        auto ec = readable_field_traits<ReadableField>::parse(fields_[*pos_map_++], output);
        if (!ec_)
            ec_ = ec;
    }

    error_code error() const noexcept { return ec_; }
};

constexpr std::size_t get_length(const char* s) noexcept
{
    const char* p = s;
    while (*p)
        ++p;
    return p - s;
}

// Base template
template <class RowType, bool is_describe_struct = boost::describe::has_describe_members<RowType>::value>
struct row_traits
{
    static constexpr bool is_supported = false;
};

// Describe structs
template <class RowType>
using row_members = boost::describe::
    describe_members<RowType, boost::describe::mod_public | boost::describe::mod_inherited>;

template <class MemberDescriptor>
constexpr string_view get_member_name(MemberDescriptor d) noexcept
{
    return string_view(d.name, get_length(d.name));
}

template <template <class...> class ListType, class... MemberDescriptor>
constexpr array_wrapper<string_view, sizeof...(MemberDescriptor)> get_describe_names(ListType<
                                                                                     MemberDescriptor...>)
{
    return {{get_member_name(MemberDescriptor())...}};
}

template <class RowType>
constexpr auto describe_names_storage = get_describe_names(row_members<RowType>{});

template <class RowType>
class row_traits<RowType, true>
{
    using members = row_members<RowType>;

    template <class D>
    struct descriptor_to_type
    {
        using helper = decltype(std::declval<RowType>().*std::declval<D>().pointer);
        using type = typename std::remove_reference<helper>::type;
    };

    using member_types = mp11::mp_transform<descriptor_to_type, members>;

    static_assert(check_readable_field<member_types>(), "");

public:
    static constexpr std::size_t size() noexcept { return boost::mp11::mp_size<members>::value; }

    // TODO: allow disabling matching by name
    static constexpr const string_view* field_names() noexcept
    {
        return describe_names_storage<RowType>.data;
    }

    static void meta_check(meta_check_context& ctx)
    {
        boost::mp11::mp_for_each<member_types>([&](auto type_identity) {
            meta_check_field<typename decltype(type_identity)::type>(ctx);
        });
    }

    static void parse(parse_functor& parser, RowType& to)
    {
        boost::mp11::mp_for_each<members>([&](auto D) { parser(to.*D.pointer); });
    }
};

// Tuples
template <class... FieldType>
class row_traits<std::tuple<FieldType...>, false>
{
    using tuple_type = std::tuple<FieldType...>;
    using field_types = boost::mp11::mp_list<boost::mp11::mp_identity<FieldType>...>;

    static_assert(check_readable_field<field_types>(), "");

public:
    static constexpr std::size_t size() noexcept { return std::tuple_size<tuple_type>::value; }
    static constexpr const string_view* field_names() noexcept { return nullptr; }

    static void meta_check(meta_check_context& ctx)
    {
        boost::mp11::mp_for_each<field_types>([&ctx](auto type_identity) {
            meta_check_field<typename decltype(type_identity)::type>(ctx);
        });
    }

    static void parse(parse_functor& parser, tuple_type& to) { boost::mp11::tuple_for_each(to, parser); }
};

template <class RowType>
struct is_row_type
{
    static constexpr bool value = row_traits<RowType>::is_supported;
};

#ifdef BOOST_MYSQL_HAS_CONCEPTS

template <class T>
concept row_type = is_row_type<T>::value;

#define BOOST_MYSQL_ROW_TYPE ::boost::mysql::detail::row_type

#else
#define BOOST_MYSQL_ROW_TYPE class
#endif

// External interface
template <class RowType>
constexpr std::size_t get_row_size()
{
    return row_traits<RowType>::size();
}

template <class RowType>
constexpr const string_view* get_row_field_names()
{
    return row_traits<RowType>::field_names();
}

template <class RowType>
error_code meta_check(
    metadata_collection_view meta,
    const string_view* field_names,
    const std::size_t* pos_map,
    diagnostics& diag
)
{
    meta_check_context ctx(meta.data(), field_names, pos_map);
    row_traits<RowType>::meta_check(ctx);
    return ctx.check_errors(diag);
}

template <class RowType>
error_code parse(const field_view* from, const std::size_t* pos_map, RowType& to)
{
    parse_functor ctx(from, pos_map);
    row_traits<RowType>::parse(ctx, to);
    return ctx.error();
}

using meta_check_fn =
    error_code (*)(metadata_collection_view, const string_view*, const std::size_t*, diagnostics&);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
