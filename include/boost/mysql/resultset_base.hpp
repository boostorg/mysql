//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_RESULTSET_BASE_HPP
#define BOOST_MYSQL_RESULTSET_BASE_HPP

#include <boost/mysql/detail/protocol/common_messages.hpp>
#include <boost/mysql/detail/protocol/deserialization_context.hpp>
#include <boost/mysql/detail/protocol/resultset_encoding.hpp>
#include <boost/mysql/error.hpp>
#include <boost/mysql/metadata.hpp>
#include <boost/mysql/metadata_collection_view.hpp>
#include <boost/mysql/row.hpp>

#include <boost/utility/string_view.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace boost {
namespace mysql {

/**
 * \brief The base class for resultsets.
 * \details Don't instantiate this class directly - use \ref resultset instead.
 *\n
 * All member functions, except otherwise noted, have `this->valid()` as precondition.
 * Calling any function on an invalid resultset results in undefined behavior.
 */
class resultset_base
{
    class ok_packet_data
    {
        bool has_data_{false};
        std::uint64_t affected_rows_;
        std::uint64_t last_insert_id_;
        std::uint16_t warnings_;
        std::string info_;

    public:
        ok_packet_data() = default;
        ok_packet_data(const detail::ok_packet& pack) { assign(pack); }

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
        std::uint64_t affected_rows() const noexcept
        {
            assert(has_data_);
            return affected_rows_;
        }
        std::uint64_t last_insert_id() const noexcept
        {
            assert(has_data_);
            return last_insert_id_;
        }
        unsigned warning_count() const noexcept
        {
            assert(has_data_);
            return warnings_;
        }
        boost::string_view info() const noexcept
        {
            assert(has_data_);
            return info_;
        }
    };

    void* channel_{nullptr};
    std::uint8_t seqnum_{};
    detail::resultset_encoding encoding_{detail::resultset_encoding::text};
    std::vector<metadata> meta_;
    ok_packet_data ok_packet_;

public:
#ifndef BOOST_MYSQL_DOXYGEN
    // Private, do not use. TODO: hide these
    resultset_base() = default;
    void reset(void* channel, detail::resultset_encoding encoding) noexcept
    {
        channel_ = channel;
        seqnum_ = 0;
        encoding_ = encoding;
        meta_.clear();
        ok_packet_.reset();
    }

    void complete(const detail::ok_packet& ok_pack)
    {
        assert(valid());
        ok_packet_.assign(ok_pack);
    }

    void prepare_meta(std::size_t num_fields) { meta_.reserve(num_fields); }

    void add_meta(const detail::column_definition_packet& pack) { meta_.emplace_back(pack, true); }

    detail::resultset_encoding encoding() const noexcept { return encoding_; }

    std::uint8_t& sequence_number() noexcept { return seqnum_; }

    std::vector<metadata>& fields() noexcept { return meta_; }
    const std::vector<metadata>& fields() const noexcept { return meta_; }
#endif

    /**
     * \brief Returns `true` if the object represents an actual resultset.
     * \details Calling any function other than assignment on a resultset for which
     * this function returns `false` results in undefined behavior.
     *
     * To be usable for server communication, the \ref connection referenced by this object must be
     * alive and open, too.
     *
     * Returns `false` for default-constructed and moved-from objects.
     */
    bool valid() const noexcept { return channel_ != nullptr; }

    /**
     * \brief Returns whether the resultset has been completely read or not.
     * \details
     * After a resultset is `complete`, you may access extra information about the operation, like
     * \ref affected_rows or \ref last_insert_id.
     */
    bool complete() const noexcept { return ok_packet_.has_value(); }

    /**
     * \brief Returns [link mysql.resultsets.metadata metadata] about the columns in the query.
     * \details
     * The returned collection will have as many \ref metadata objects as columns retrieved by
     * the SQL query, and in the same order.
     *\n
     * This function returns a view object, with reference semantics. This view object references
     * `*this` internal state, and will be valid as long as `*this` (or a `resultset`
     * move-constructed from `*this`) is alive.
     */
    metadata_collection_view meta() const noexcept
    {
        return metadata_collection_view(meta_.data(), meta_.size());
    }

    /**
     * \brief The number of rows affected by the SQL statement that generated this resultset.
     * \details The resultset **must be complete**
     * before calling this function.
     */
    std::uint64_t affected_rows() const noexcept { return ok_packet_.affected_rows(); }

    /**
     * \brief The last insert ID produced by the SQL statement that generated this resultset.
     * \details The resultset **must be complete** before calling this function.
     */
    std::uint64_t last_insert_id() const noexcept { return ok_packet_.last_insert_id(); }

    /**
     * \brief The number of warnings produced by the SQL statement that generated this resultset.
     * \details The resultset **must be complete** before calling this function.
     */
    unsigned warning_count() const noexcept { return ok_packet_.warning_count(); }

    /**
     * \brief Additionat text information about the execution of
     *        the SQL statement that generated this resultset.
     * \details The resultset **must be complete** before calling this function.
     *\n
     * This function returns a view object, with reference semantics. This view object references
     * `*this` internal state, and will be valid as long as `*this` (or a `resultset`
     * move-constructed from `*this`) is alive.
     */
    boost::string_view info() const noexcept { return ok_packet_.info(); }

protected:
    void* channel_ptr() noexcept { return channel_; }
    void reset() noexcept { reset(nullptr, detail::resultset_encoding::text); }
    void swap(resultset_base& other) noexcept { std::swap(*this, other); }
};

}  // namespace mysql
}  // namespace boost

#endif
