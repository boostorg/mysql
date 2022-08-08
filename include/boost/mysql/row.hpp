//
// Copyright (c) 2019-2022 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ROW_HPP
#define BOOST_MYSQL_ROW_HPP

#include <boost/mysql/field_view.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/utility/string_view_fwd.hpp>
#include <vector>


namespace boost {
namespace mysql {

/**
 * \brief Represents a row returned from a database operation.
 * \details A row is a collection of values, plus a buffer holding memory
 * for the string [reflink value]s.
 *
 * Call [refmem row values] to get the actual sequence of
 * [reflink value]s the row contains.
 *
 * There will be the same number of values and in the same order as fields
 * in the SQL query that produced the row. You can get more information
 * about these fields using [refmem resultset fields].
 *
 * If any of the values is a string, it will be represented as a `string_view`
 * pointing into the row's buffer. These string values will be valid as long as
 * the [reflink row] object containing the memory they point to is alive and valid. Concretely:
 * - Destroying the row object invalidates the string values.
 * - Move assigning against the row invalidates the string values.
 * - Calling [refmem row clear] invalidates the string values.
 * - Move-constructing a [reflink row] from the current row does **not**
 *   invalidate the string values.
 *
 * Default constructible and movable, but not copyable.
 */
class row
{
public:
    row() = default;
    inline row(const row&);
    row(row&&) = default;
    inline const row& operator=(const row&);
    row& operator=(row&&) = default;
    ~row() = default;

    using iterator = const field_view*;
    using const_iterator = iterator;
    // TODO: add other standard container members when we add field and field_view

    iterator begin() const noexcept { return fields_.data(); }
    iterator end() const noexcept { return fields_.data() + fields_.size(); }
    field_view at(std::size_t i) const { return fields_.at(i); }
    field_view operator[](std::size_t i) const noexcept { return fields_[i]; }
    field_view front() const noexcept { return fields_.front(); }
    field_view back() const noexcept { return fields_.back(); }
    bool empty() const noexcept { return fields_.empty(); }
    std::size_t size() const noexcept { return fields_.size(); }

    inline iterator insert(iterator before, field_view v);
    inline iterator insert(iterator before, std::initializer_list<field_view> v);
    template <class FwdIt>
    iterator insert(iterator before, FwdIt first, FwdIt last);

    inline iterator replace(iterator pos, field_view v);
    inline iterator replace(iterator first, iterator last, std::initializer_list<field_view> v);
    template <class FwdIt>
    inline iterator replace(iterator first, iterator last, FwdIt other_first, FwdIt other_last);

    inline iterator erase(iterator pos);
    inline iterator erase(iterator first, iterator last);

    inline void push_back(field_view v);
    inline void pop_back();

    operator row_view() const noexcept
    {
        return row_view(fields_.data(), fields_.size());
    }



    /**
     * \brief Clears the row object.
     * \details Clears the value array and the memory buffer associated to this row.
     * After calling this operation, [refmem row values] will be the empty array. Any
     * pointers, references and iterators to elements in [refmem row values] will be invalidated.
     * Any string values using the memory held by this row will also become invalid. 
     */
    void clear() noexcept
    {
        fields_.clear();
        string_buffer_.clear();
    }

    // TODO: hide these
    const std::vector<field_view>& fields() const noexcept { return fields_; }
    std::vector<field_view>& values() noexcept { return fields_; }
    inline void copy_strings();
private:
    std::vector<field_view> fields_;
    std::vector<char> string_buffer_;

    inline iterator to_row_it(std::vector<field_view>::iterator it) noexcept;
    inline std::vector<field_view>::iterator to_vector_it(iterator it) noexcept;
    inline boost::string_view copy_string(boost::string_view v);
    inline void rebase_strings(const char* old_buffer_base) noexcept;
};


inline bool operator==(const row& lhs, const row& rhs)
{
    return row_view(lhs) == row_view(rhs);
}

/**
 * \relates row
 * \brief Compares two rows.
 */
inline bool operator!=(const row& lhs, const row& rhs) { return !(lhs == rhs); }


} // mysql
} // boost

#include <boost/mysql/impl/row.ipp>


#endif
