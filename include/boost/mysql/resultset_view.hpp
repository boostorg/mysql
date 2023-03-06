//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_RESULTSET_VIEW_HPP
#define BOOST_MYSQL_RESULTSET_VIEW_HPP

#include <boost/mysql/detail/protocol/execution_state_impl.hpp>

namespace boost {
namespace mysql {

class resultset_view
{
    const detail::execution_state_impl* st_{};
    std::size_t index_{};

public:
    resultset_view() = default;

    // TODO: hide this
    resultset_view(const detail::execution_state_impl& st, std::size_t index) noexcept
        : st_(&st), index_(index)
    {
    }

    bool has_value() const noexcept { return st_ != nullptr; }

    rows_view rows() const noexcept
    {
        assert(has_value());
        return st_->get_rows(index_);
    }

    metadata_collection_view meta() const noexcept
    {
        assert(has_value());
        return st_->get_meta(index_);
    }

    std::uint64_t affected_rows() const noexcept
    {
        assert(has_value());
        return st_->get_affected_rows(index_);
    }

    std::uint64_t last_insert_id() const noexcept
    {
        assert(has_value());
        return st_->get_last_insert_id(index_);
    }

    unsigned warning_count() const noexcept
    {
        assert(has_value());
        return st_->get_warning_count(index_);
    }

    string_view info() const noexcept
    {
        assert(has_value());
        return st_->get_info(index_);
    }

    bool is_out_params() const noexcept
    {
        assert(has_value());
        return st_->get_is_out_params(index_);
    }

    const resultset_view* operator->() const noexcept { return this; }
};

}  // namespace mysql
}  // namespace boost

#endif
