//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_METADATA_COLLECTION_VIEW_HPP
#define BOOST_MYSQL_METADATA_COLLECTION_VIEW_HPP

#include <boost/mysql/metadata.hpp>

#include <cstddef>
#include <stdexcept>

namespace boost {
namespace mysql {

/**
 * \brief A read-only view of a collection of \ref metadata objects.
 * \details
 * The object doesn't own the storage for the \ref metadata objects. These are typically
 * owned by a \ref resultset object. This view is valid as long as the memory allocated for the \ref
 * metadata objects remain valid. See \ref resultset for more details.
 */
class metadata_collection_view
{
    const metadata* data_{};
    std::size_t size_{};

public:
    /**
     * \brief Constructs an empty view.
     */
    metadata_collection_view() = default;

    // TODO: hide this
    metadata_collection_view(const metadata* data, std::size_t size) noexcept
        : data_(data), size_(size)
    {
    }

#ifdef BOOST_MYSQL_DOXYGEN
    /**
     * \brief A random access iterator to an element.
     * \details The exact type of the iterator is unspecified.
     */
    using iterator = __see_below__;
#else
    using iterator = const metadata*;
#endif

    /// \copydoc iterator
    using const_iterator = iterator;

    /// The value type.
    using value_type = metadata;

    /// The reference type.
    using reference = const metadata&;

    /// The const reference type.
    using const_reference = reference;

    /// An unsigned integer type to represent sizes.
    using size_type = std::size_t;

    /// A signed integer type used to represent differences.
    using difference_type = std::ptrdiff_t;

    /**
     * \brief Returns an iterator to the first \ref metadata object in the collection.
     */
    iterator begin() const noexcept { return data_; }

    /**
     * \brief Returns an iterator to one-past-the-last object in the collection.
     */
    iterator end() const noexcept { return data_ + size_; }

    /**
     * \brief Returns a reference to the i-th element or throws an exception.
     * \details Throws `std::out_of_range` if `i >= this->size()`.
     */
    const metadata& at(std::size_t i) const
    {
        if (i >= size_)
            throw std::out_of_range("metadata_collection_view::at");
        return data_[i];
    }

    /**
     * \brief Returns a reference to the i-th element (unchecked access).
     * \details Results in undefined behavior if `i >= this->size()`.
     */
    const metadata& operator[](std::size_t i) const noexcept { return data_[i]; }

    /**
     * \brief Returns true if there are no elements in the collection (i.e. `this->size() == 0`)
     */
    bool empty() const noexcept { return size_ == 0; }

    /**
     * \brief Returns the number of elements in the collection.
     */
    std::size_t size() const noexcept { return size_; }
};

}  // namespace mysql
}  // namespace boost

#endif
