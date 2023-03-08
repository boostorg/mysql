//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_RESULTSET_HPP
#define BOOST_MYSQL_RESULTSET_HPP

#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/resultset_view.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/rows.hpp>

#include <cassert>

namespace boost {
namespace mysql {

class resultset
{
public:
    resultset() = default;

    resultset(resultset_view v) { assign(v); }

    resultset& operator=(resultset_view v)
    {
        assign(v);
        return *this;
    }

    bool has_value() const noexcept { return has_value_; }

    rows_view rows() const noexcept
    {
        assert(has_value_);
        return rws_;
    }

    metadata_collection_view meta() const noexcept
    {
        assert(has_value_);
        return meta_;
    }

    std::uint64_t affected_rows() const noexcept
    {
        assert(has_value_);
        return affected_rows_;
    }

    std::uint64_t last_insert_id() const noexcept
    {
        assert(has_value_);
        return last_insert_id_;
    }

    unsigned warning_count() const noexcept
    {
        assert(has_value_);
        return warnings_;
    }

    string_view info() const noexcept
    {
        assert(has_value_);
        return string_view(info_.data(), info_.size());
    }

    bool is_out_params() const noexcept
    {
        assert(has_value_);
        return is_out_params_;
    }

private:
    bool has_value_{false};
    std::vector<metadata> meta_;
    ::boost::mysql::rows rws_;
    std::uint64_t affected_rows_{};
    std::uint64_t last_insert_id_{};
    std::uint16_t warnings_{};
    std::vector<char> info_;
    bool is_out_params_{false};

    void assign(resultset_view v)
    {
        has_value_ = v.has_value();
        if (has_value_)
        {
            meta_.assign(v.meta().begin(), v.meta().end());
            rws_ = v.rows();
            affected_rows_ = v.affected_rows();
            last_insert_id_ = v.last_insert_id();
            warnings_ = v.warning_count();
            info_.assign(v.info().begin(), v.info().end());
            is_out_params_ = v.is_out_params();
        }
        else
        {
            meta_.clear();
            rws_ = ::boost::mysql::rows();
            affected_rows_ = 0;
            last_insert_id_ = 0;
            warnings_ = 0;
            info_.clear();
            is_out_params_ = false;
        }
    }
};

}  // namespace mysql
}  // namespace boost

#endif
