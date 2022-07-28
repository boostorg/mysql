//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_RESULTSET_HPP
#define BOOST_MYSQL_RESULTSET_HPP

#include <boost/mysql/row.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/auxiliar/bytestring.hpp>
#include <boost/mysql/detail/network_algorithms/common.hpp> // deserialize_row_fn
#include <boost/utility/string_view_fwd.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace boost {
namespace mysql {

/**
 * \brief Represents tabular data retrieved from the MySQL server.
 *        See [link mysql.resultsets this section] for more info.
 * \details Returned as the result of a [link mysql.queries text query]
 * or a [link mysql.prepared_statements statement execution]. It is an I/O object
 * that allows reading rows progressively. [link mysql.resultsets This section]
 * provides an in-depth explanation of the mechanics of this class.
 *
 * Resultsets are default-constructible and movable, but not copyable. 
 * [refmem resultset valid] returns `false` for default-constructed 
 * and moved-from resultsets. Calling any member function on an invalid
 * resultset, other than assignment, results in undefined behavior.
 */
class resultset
{
    class ok_packet_data
    {
        bool has_data_ {false};
        std::uint64_t affected_rows_;
        std::uint64_t last_insert_id_;
        std::uint16_t warnings_;
        std::string info_;
    public:
        ok_packet_data() = default;
        ok_packet_data(const detail::ok_packet& pack)
        {
            assign(pack);
        }

        void reset() noexcept { has_data_ = false; }
        void assign(const detail::ok_packet& pack)
        {
            has_data_ = true;
            affected_rows_ = pack.affected_rows.value;
            last_insert_id_ = pack.last_insert_id.value;
            warnings_ = pack.warnings;
            info_.assign(pack.info.value.begin(), pack.info.value.end());
        }

        bool has_value() const noexcept { return has_data_; }
        std::uint64_t affected_rows() const noexcept { assert(has_data_); return affected_rows_; }
        std::uint64_t last_insert_id() const noexcept { assert(has_data_); return last_insert_id_; }
        unsigned warning_count() const noexcept { assert(has_data_); return warnings_; }
        boost::string_view info() const noexcept { assert(has_data_); return info_; }
    };

    bool valid_ {false};
    std::uint8_t seqnum_ {};
    detail::deserialize_row_fn deserializer_ {};
    std::vector<field_metadata> meta_;
    ok_packet_data ok_packet_;

public:
    /// \brief Default constructor.
    /// \details Default constructed resultsets have [refmem resultset valid] return `false`.
    resultset() = default;

#ifndef BOOST_MYSQL_DOXYGEN
    // Private, do not use. TODO: hide these
    resultset(std::vector<field_metadata>&& meta, detail::deserialize_row_fn deserializer) noexcept:
        valid_(true),
        deserializer_(deserializer),
        meta_(std::move(meta))
    {
    };
    resultset(const detail::ok_packet& ok_pack):
        valid_(true),
        ok_packet_(ok_pack)
    {
    };

    void reset(
        detail::deserialize_row_fn deserializer
    )
    {
        valid_ = true;
        seqnum_ = 0;
        deserializer_ = deserializer;
        meta_.clear();
        ok_packet_.reset();
    }

    void complete(const detail::ok_packet& ok_pack)
    {
        assert(valid_);
        ok_packet_.assign(ok_pack);
    }

    void prepare_meta(std::size_t num_fields)
    {
        meta_.resize(num_fields);
    }

    void add_meta(const detail::column_definition_packet& pack)
    {
        meta_.emplace_back(pack, true);
    }

    std::uint8_t& sequence_number() noexcept { return seqnum_; }

    std::vector<field_metadata>& meta() noexcept { return meta_; }
#endif

    /**
     * \brief Returns whether this object represents a valid resultset.
     * \details Returns `false` for default-constructed and moved-from resultsets.
     * Calling any member function on an invalid resultset,
     * other than assignment, results in undefined behavior.
     */
    bool valid() const noexcept { return valid_; }

    /// \brief Returns whether the resultset has been completely read or not.
    /// \details See [link mysql.resultsets.complete this section] for more info.
    bool complete() const noexcept { return ok_packet_.has_value(); }

    // TODO: hide this
    /**
     * \brief Returns [link mysql.resultsets.metadata metadata] about the fields in the query.
     * \details There will be as many [reflink field_metadata] objects as fields
     * in the SQL query, and in the same order.
     */
    const std::vector<field_metadata>& fields() const noexcept { return meta_; }

    // TODO: hide this
    std::vector<field_metadata>& fields() noexcept { return meta_; }

    /**
     * \brief The number of rows affected by the SQL that generated this resultset.
     * \details The resultset __must be [link mysql.resultsets.complete complete]__
     * before calling this function.
     */
    std::uint64_t affected_rows() const noexcept { return ok_packet_.affected_rows(); }

    /**
     * \brief The last insert ID produced by the SQL that generated this resultset.
     * \details The resultset __must be [link mysql.resultsets.complete complete]__
     * before calling this function.
     */
    std::uint64_t last_insert_id() const noexcept { return ok_packet_.last_insert_id(); }

    /**
     * \brief The number of warnings produced by the SQL that generated this resultset.
     * \details The resultset __must be [link mysql.resultsets.complete complete]__
     *  before calling this function.
     */
    unsigned warning_count() const noexcept { return ok_packet_.warning_count(); }

    /**
     * \brief Additionat text information about the execution of
     *        the SQL that generated this resultset.
     * \details The resultset __must be [link mysql.resultsets.complete complete]__
     * before calling this function. The returned string is guaranteed to be valid
     * until the resultset object is destroyed.
     */
    boost::string_view info() const noexcept { return ok_packet_.info(); }
};

} // mysql
} // boost

#endif
