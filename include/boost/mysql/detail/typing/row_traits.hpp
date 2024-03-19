//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPING_ROW_TRAITS_HPP
#define BOOST_MYSQL_DETAIL_TYPING_ROW_TRAITS_HPP

#include <boost/mysql/detail/config.hpp>

#ifdef BOOST_MYSQL_CXX14

#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/get_row_type.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/typing/meta_check_context.hpp>
#include <boost/mysql/detail/typing/pos_map.hpp>
#include <boost/mysql/detail/typing/readable_field_traits.hpp>

#include <boost/assert.hpp>
#include <boost/describe/members.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

//
// Base templates. Every StaticRow type must:
//   - Specialize is_static_row to true.
//   - Specialize underlying_row (optional), which supports marker types like pfr_by_name.
//     This is in a separate header because it's included in connection, which shouldn't include all the
//     static interface.
//   - Specialize the row_traits class, providing the following members:
//
//         using types = /* mp11 list with the row's member types */ ;
//
//         static constexpr name_table_t name_table() noexcept; // field names
//
//         /* Apply F to each member */
//         template <class F>
//         static void for_each_member(underlying_row_t<StaticRow>& to, F&& function);
//
template <class T>
constexpr bool is_static_row = describe::has_describe_members<T>::value;

template <class T, bool is_describe_struct = describe::has_describe_members<T>::value>
class row_traits;

//
// Describe structs
//
// Workaround std::array::data not being constexpr in C++14
template <class T, std::size_t N>
struct array_wrapper
{
    T data_[N];

    constexpr boost::span<const T> span() const noexcept { return boost::span<const T>(data_); }
};

template <class T>
struct array_wrapper<T, 0>
{
    struct
    {
    } data_;  // allow empty brace initialization

    constexpr boost::span<const T> span() const noexcept { return boost::span<const T>(); }
};

// Workaround for char_traits::length not being constexpr in C++14
// Only used to retrieve Describe member name lengths
constexpr std::size_t get_length(const char* s) noexcept
{
    const char* p = s;
    while (*p)
        ++p;
    return p - s;
}

template <class DescribeStruct>
using row_members = describe::
    describe_members<DescribeStruct, describe::mod_public | describe::mod_inherited>;

template <template <class...> class ListType, class... MemberDescriptor>
constexpr array_wrapper<string_view, sizeof...(MemberDescriptor)> get_describe_names(ListType<
                                                                                     MemberDescriptor...>)
{
    return {{string_view(MemberDescriptor::name, get_length(MemberDescriptor::name))...}};
}

template <class DescribeStruct>
constexpr auto describe_names_storage = get_describe_names(row_members<DescribeStruct>{});

template <class DescribeStruct>
class row_traits<DescribeStruct, true>
{
    using members = row_members<DescribeStruct>;

    // clang-format off
    template <class D>
    using descriptor_to_type = typename
        std::remove_reference<decltype(std::declval<DescribeStruct>().*std::declval<D>().pointer)>::type;
    // clang-format on

public:
    using types = mp11::mp_transform<descriptor_to_type, members>;

    static constexpr name_table_t name_table() noexcept
    {
        return describe_names_storage<DescribeStruct>.span();
    }

    template <class F>
    static void for_each_member(DescribeStruct& to, F&& function)
    {
        mp11::mp_for_each<members>([function, &to](auto D) { function(to.*D.pointer); });
    }
};

//
// Tuples
//
template <class... T>
constexpr bool is_static_row<std::tuple<T...>> = true;

template <class... ReadableField>
class row_traits<std::tuple<ReadableField...>, false>
{
public:
    using types = std::tuple<ReadableField...>;
    static constexpr name_table_t name_table() noexcept { return name_table_t(); }

    template <class F>
    static void for_each_member(std::tuple<ReadableField...>& to, F&& function)
    {
        mp11::tuple_for_each(to, std::forward<F>(function));
    }
};

//
// Helpers to implement the external interface section
//

// Helpers to check that all the fields satisfy ReadableField
// and produce meaningful error messages, with the offending field type, at least
// Workaround clang 3.6 not liking generic lambdas in the below constexpr function
struct readable_field_checker
{
    template <class TypeIdentity>
    constexpr void operator()(TypeIdentity) const noexcept
    {
        using T = typename TypeIdentity::type;
        static_assert(
            is_readable_field<T>::value,
            "You're trying to use an unsupported field type in a row type. Review your row type definitions."
        );
    }
};

template <class TypeList>
static constexpr bool check_readable_field() noexcept
{
    mp11::mp_for_each<mp11::mp_transform<mp11::mp_identity, TypeList>>(readable_field_checker{});
    return true;
}

// Centralize where check_readable_field happens
template <class StaticRow>
struct row_traits_with_check : row_traits<StaticRow>
{
    static_assert(check_readable_field<typename row_traits<StaticRow>::types>(), "");
};

class parse_context
{
    span<const std::size_t> pos_map_;
    span<const field_view> fields_;
    std::size_t index_{};
    error_code ec_;

public:
    parse_context(span<const std::size_t> pos_map, span<const field_view> fields) noexcept
        : pos_map_(pos_map), fields_(fields)
    {
    }

    template <class ReadableField>
    void parse(ReadableField& output)
    {
        auto ec = readable_field_traits<ReadableField>::parse(
            map_field_view(pos_map_, index_++, fields_),
            output
        );
        if (!ec_)
            ec_ = ec;
    }

    error_code error() const noexcept { return ec_; }
};

// Using this instead of a lambda reduces the number of generated instantiations
struct parse_functor
{
    parse_context& ctx;

    template <class ReadableField>
    void operator()(ReadableField& output) const
    {
        ctx.parse(output);
    }
};

//
// External interface. Other Boost.MySQL components should never use row_traits
// directly, but the functions below, instead.
//

#ifdef BOOST_MYSQL_HAS_CONCEPTS

// Note that static_row only inspects the shape of the row only (i.e. it's a tuple vs. it's nothing we know),
// and not individual fields. These are static_assert-ed in individual row_traits. This gives us an error
// message that contains the offending types, at least.
template <class T>
concept static_row = is_static_row<T>;

#define BOOST_MYSQL_STATIC_ROW ::boost::mysql::detail::static_row

#else
#define BOOST_MYSQL_STATIC_ROW class
#endif

template <BOOST_MYSQL_STATIC_ROW StaticRow>
constexpr std::size_t get_row_size()
{
    return mp11::mp_size<typename row_traits_with_check<StaticRow>::types>::value;
}

template <BOOST_MYSQL_STATIC_ROW StaticRow>
constexpr name_table_t get_row_name_table()
{
    return row_traits_with_check<StaticRow>::name_table();
}

template <BOOST_MYSQL_STATIC_ROW StaticRow>
error_code meta_check(span<const std::size_t> pos_map, metadata_collection_view meta, diagnostics& diag)
{
    using fields = typename row_traits_with_check<StaticRow>::types;
    BOOST_ASSERT(pos_map.size() == get_row_size<StaticRow>());
    return meta_check_field_type_list<fields>(pos_map, get_row_name_table<StaticRow>(), meta, diag);
}

template <BOOST_MYSQL_STATIC_ROW StaticRow>
error_code parse(
    span<const std::size_t> pos_map,
    span<const field_view> from,
    underlying_row_t<StaticRow>& to
)
{
    BOOST_ASSERT(pos_map.size() == get_row_size<StaticRow>());
    BOOST_ASSERT(from.size() >= get_row_size<StaticRow>());
    parse_context ctx(pos_map, from);
    row_traits_with_check<StaticRow>::for_each_member(to, parse_functor{ctx});
    return ctx.error();
}

using meta_check_fn_t =
    error_code (*)(span<const std::size_t> field_map, metadata_collection_view meta, diagnostics& diag);

// For multi-resultset - helper
template <class... StaticRow>
constexpr std::size_t max_num_columns = (std::max)({get_row_size<StaticRow>()...});

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif  // BOOST_MYSQL_CXX14

#endif
