//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPING_ROW_TRAITS_HPP
#define BOOST_MYSQL_DETAIL_TYPING_ROW_TRAITS_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/metadata_collection_view.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/typing/field_traits.hpp>

#include <boost/describe/members.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/utility.hpp>

#include <tuple>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

// Helpers
class parse_functor
{
    const field_view* it_;
    error_code ec_;

public:
    parse_functor(const field_view* it) noexcept : it_(it) {}

    template <BOOST_MYSQL_FIELD_TYPE FieldType>
    void operator()(FieldType& output)
    {
        auto ec = field_traits<FieldType>::parse(*it_++, output);
        if (!ec_)
            ec_ = ec;
    }

    error_code error() const noexcept { return ec_; }
};

template <class RowType, bool is_describe_struct = boost::describe::has_describe_members<RowType>::value>
struct row_traits
{
    static constexpr bool is_supported = false;
};

// Describe structs
template <class RowType>
class row_traits<RowType, true>
{
    using members = boost::describe::
        describe_members<RowType, boost::describe::mod_public | boost::describe::mod_inherited>;

    template <class D>
    struct descriptor_to_type
    {
        using helper = decltype(std::declval<RowType>().*std::declval<D>().pointer);
        using type = typename std::remove_reference<helper>::type;
    };

    template <class D>
    struct is_field_type_helper : is_field_type<typename descriptor_to_type<D>::type>
    {
    };

    static_assert(
        boost::mp11::mp_all_of<members, is_field_type_helper>::value,
        "Some types in your struct are not valid field types"
    );

public:
    static constexpr std::size_t size = boost::mp11::mp_size<members>::value;

    static void meta_check(meta_check_context& ctx)
    {
        boost::mp11::mp_for_each<members>([&](auto D) {
            using T = typename descriptor_to_type<decltype(D)>::type;
            meta_check_impl<T>(ctx);
        });
    }

    static void parse(parse_functor& parser, RowType& to)
    {
        boost::mp11::mp_for_each<members>([&](auto D) { parser(to.*D.pointer); });
    }
};

// Tuples
struct tuple_meta_check_fn
{
    meta_check_context& ctx;

    template <class FieldType>
    void operator()(boost::mp11::mp_identity<FieldType>) const
    {
        meta_check_impl<FieldType>(ctx);
    }
};

template <class... FieldType>
class row_traits<std::tuple<FieldType...>, false>
{
    using tuple_type = std::tuple<FieldType...>;
    static_assert(
        boost::mp11::mp_all_of<tuple_type, is_field_type>::value,
        "Some types in your std::tuple<...> are not valid field types"
    );

public:
    static constexpr std::size_t size = std::tuple_size<tuple_type>::value;

    static void meta_check(meta_check_context& ctx)
    {
        using types = boost::mp11::mp_list<boost::mp11::mp_identity<FieldType>...>;
        boost::mp11::mp_for_each<types>(tuple_meta_check_fn{ctx});
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
    return row_traits<RowType>::size;
}

template <class RowType>
error_code meta_check(metadata_collection_view meta, diagnostics& diag)
{
    meta_check_context ctx(meta.data());
    row_traits<RowType>::meta_check(ctx);
    return ctx.check_errors(diag);
}

template <class RowType>
error_code parse(const field_view* from, RowType& to)
{
    parse_functor ctx(from);
    row_traits<RowType>::parse(ctx, to);
    return ctx.error();
}

using meta_check_fn = error_code (*)(metadata_collection_view, diagnostics&);

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
