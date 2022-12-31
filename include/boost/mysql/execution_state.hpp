//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_EXECUTION_STATE_HPP
#define BOOST_MYSQL_EXECUTION_STATE_HPP

#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace boost {
namespace mysql {

/**
 * \brief Holds state for multi-function SQL execution operations.
 */
class execution_state
{
    bool eof_received_{false};
    std::uint8_t seqnum_{};
    detail::resultset_encoding encoding_{detail::resultset_encoding::text};
    std::vector<metadata> meta_;
    std::uint64_t affected_rows_{};
    std::uint64_t last_insert_id_{};
    std::uint16_t warnings_{};
    std::vector<char> info_;  // guarantee that no SBO is used

public:
#ifndef BOOST_MYSQL_DOXYGEN
    // Private, do not use. TODO: hide these
    void reset(detail::resultset_encoding encoding) noexcept
    {
        seqnum_ = 0;
        encoding_ = encoding;
        meta_.clear();
        eof_received_ = false;
    }

    void complete(const detail::ok_packet& pack)
    {
        affected_rows_ = pack.affected_rows.value;
        last_insert_id_ = pack.last_insert_id.value;
        warnings_ = pack.warnings;
        info_.assign(pack.info.value.begin(), pack.info.value.end());
        eof_received_ = true;
    }

    void prepare_meta(std::size_t num_fields) { meta_.reserve(num_fields); }

    void add_meta(const detail::column_definition_packet& pack) { meta_.emplace_back(pack, true); }

    detail::resultset_encoding encoding() const noexcept { return encoding_; }

    std::uint8_t& sequence_number() noexcept { return seqnum_; }

    std::vector<metadata>& fields() noexcept { return meta_; }
    const std::vector<metadata>& fields() const noexcept { return meta_; }
#endif

    /**
     * \brief Default constructor.
     * \details The constructed object is guaranteed to have `meta().empty()` and
     * `!complete()`.
     */
    execution_state() = default;

    /**
     * \brief Returns whether the resultset generated by this operation has been completely read.
     * \details
     * Once `complete`, you may access extra information about the operation, like
     * \ref affected_rows or \ref last_insert_id.
     */
    bool complete() const noexcept { return eof_received_; }

    /**
     * \brief Returns metadata about the columns in the query.
     * \details
     * The returned collection will have as many \ref metadata objects as columns retrieved by
     * the SQL query, and in the same order.
     *\n
     * This function returns a view object, with reference semantics. The returned view points into
     * memory owned by `*this`, and will be valid as long as `*this` or an object move-constructed
     * from `*this` are alive.
     */
    metadata_collection_view meta() const noexcept
    {
        return metadata_collection_view(meta_.data(), meta_.size());
    }

    /**
     * \brief Returns the number of rows affected by the executed SQL statement.
     * \details Precondition: `this->complete() == true`.
     */
    std::uint64_t affected_rows() const noexcept
    {
        assert(complete());
        return affected_rows_;
    }

    /**
     * \brief Returns the last insert ID produced by the executed SQL statement.
     * \details Precondition: `this->complete() == true`.
     */
    std::uint64_t last_insert_id() const noexcept
    {
        assert(complete());
        return last_insert_id_;
    }

    /**
     * \brief Returns the number of warnings produced by the executed SQL statement.
     * \details Precondition: `this->complete() == true`.
     */
    unsigned warning_count() const noexcept
    {
        assert(complete());
        return warnings_;
    }

    /**
     * \brief Returns additionat text information about the execution of the SQL statement.
     * \details Precondition: `this->complete() == true`.
     *\n
     * The format of this information is documented by MySQL <a
     * href="https://dev.mysql.com/doc/c-api/8.0/en/mysql-info.html">here</a>.
     *\n
     * The returned string always uses ASCII encoding, regardless of the connection's character set.
     *\n
     * This function returns a view object, with reference semantics. The returned view points into
     * memory owned by `*this`, and will be valid as long as `*this` or an object move-constructed
     * from `*this` are alive.
     */
    string_view info() const noexcept
    {
        assert(complete());
        return string_view(info_.data(), info_.size());
    }
};

}  // namespace mysql
}  // namespace boost

#endif
