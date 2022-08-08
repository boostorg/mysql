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

class metadata_collection_view
{
    const metadata* data_ {};
    std::size_t size_ {};
public:
    metadata_collection_view() = default;
    metadata_collection_view(const metadata* data, std::size_t size) noexcept :
        data_(data), size_(size) {}
    
    using iterator = const metadata*;
    using const_iterator = iterator;
    using value_type = metadata;
    using reference = const metadata&;
    using const_reference = reference;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    iterator begin() const noexcept { return data_; }
    iterator end() const noexcept { return data_ + size_; }

    const metadata& at(std::size_t i) const
    {
        if (i >= size_)
            throw std::out_of_range("metadata_collection_view::at");
        return data_[i];
    }
    const metadata& operator[](std::size_t i) const noexcept { return data_[i]; }
    bool empty() const noexcept { return size_ == 0; }
    std::size_t size() const noexcept { return size_; }
};

} // mysql
} // boost

#endif
