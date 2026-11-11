//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_IDLE_CONNECTION_LIST_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_IDLE_CONNECTION_LIST_HPP

#include <boost/assert.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/set_hook.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

// Base class for the elements of an idle_connection_list
class idle_list_hook : public intrusive::set_base_hook<>
{
    template <class T>
    friend class idle_connection_list;

    // The element's place in the list. Assigned when the element is returned,
    // and kept while the element is out of the list, so it can be restored
    std::uint64_t return_order_{0};

    struct returned_before
    {
        bool operator()(const idle_list_hook& lhs, const idle_list_hook& rhs) const noexcept
        {
            return lhs.return_order_ < rhs.return_order_;
        }
    };
};

// The connections that can be handed out, ordered by the time they were returned to the pool.
// The most recently returned one is handed out first, so the ones that aren't needed stay idle.
// A connection may leave the list for a while and get back to its place: pings do this,
// and they must not make a connection look recently returned.
// Elements are kept in a set because a connection that gets back may belong anywhere in the order.
// T must derive from idle_list_hook. Non-owning.
template <class T>
class idle_connection_list
{
    intrusive::set<T, intrusive::compare<idle_list_hook::returned_before>> impl_;

    // The number for the next returned element. It has 64 bits on every platform,
    // so it doesn't wrap in practice: pings don't increment it, only returned connections do
    std::uint64_t next_return_order_{0};

public:
    bool empty() const noexcept { return impl_.empty(); }
    std::size_t size() const noexcept { return impl_.size(); }

    // The most recently returned element. The list must not be empty
    T& back() noexcept
    {
        BOOST_ASSERT(!impl_.empty());
        return *impl_.rbegin();
    }

    // Adds an element that has just been returned. It becomes the most recently returned one
    void push_back(T& elm) noexcept
    {
        static_cast<idle_list_hook&>(elm).return_order_ = next_return_order_++;
        impl_.push_back(elm);
    }

    // Removes an element. The element remembers its place, in case it's restored
    void erase(T& elm) noexcept { impl_.erase(impl_.iterator_to(elm)); }

    // Puts an element that was removed with erase back in its place
    void restore(T& elm) noexcept { BOOST_VERIFY(impl_.insert(elm).second); }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
