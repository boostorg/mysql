//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_POOLED_CONNECTION_HPP
#define BOOST_MYSQL_POOLED_CONNECTION_HPP

#include <boost/mysql/any_connection.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connection_pool/connection_node.hpp>

#include <memory>

namespace boost {
namespace mysql {

/**
 * \brief (EXPERIMENTAL) A proxy to a connection owned by a pool that returns it to the pool when destroyed.
 * \details
 * A `pooled_connection` behaves like to a `std::unique_ptr`: it has exclusive ownership of a
 * \ref any_connection created by the pool. When destroyed, it returns the connection to the pool.
 * A `pooled_connection` may own nothing. We say it's invalid (`this->valid() == false`).
 * \n
 * This class is movable but not copyable.
 *
 * \par Object lifetimes
 * While `*this` is alive, the \ref connection_pool internal data will be kept alive
 * automatically. It's safe to destroy the `connection_pool` object before `*this`.
 *
 * \par Thread safety
 * As opposed to \ref connection_pool, individual connections created by the pool are **not**
 * thread-safe. Care must be taken not to use them in an unsafe manner.
 * \n
 * Distinct objects: safe. \n
 * Shared objects: unsafe. \n
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
class pooled_connection
{
#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif

    pooled_connection(
        detail::connection_node& node,
        std::shared_ptr<detail::connection_pool_impl> pool_impl
    ) noexcept
        : impl_(&node), pool_impl_(std::move(pool_impl))
    {
    }

    struct deleter
    {
        void operator()(detail::connection_node* node) const noexcept { node->mark_as_collectable(true); }
    };

    std::unique_ptr<detail::connection_node, deleter> impl_;
    std::shared_ptr<detail::connection_pool_impl> pool_impl_;

public:
    /**
     * \brief Constructs an invalid pooled connection.
     * \details
     * The resulting object is invalid (`this->valid() == false`).
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    pooled_connection() noexcept = default;

    /**
     * \brief Move constructor.
     * \details
     * Transfers connection ownership from `other` to `*this`.
     * \n
     * After this function returns, if `other.valid() == true`, `this->valid() == true`.
     * In any case, `other` will become invalid (`other.valid() == false`).
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    pooled_connection(pooled_connection&& other) = default;

    /**
     * \brief Move assignment.
     * \details
     * If `this->valid()`, returns the owned connection to the pool and marks
     * it as pending reset.
     * It then transfers connection ownership from `other` to `*this`.
     * \n
     * After this function returns, if `other.valid() == true`, `this->valid() == true`.
     * In any case, `other` will become invalid (`other.valid() == false`).
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    pooled_connection& operator=(pooled_connection&& other) = default;

#ifndef BOOST_MYSQL_DOXYGEN
    pooled_connection(const pooled_connection&) = delete;
    pooled_connection& operator=(const pooled_connection&) = delete;
#endif

    /**
     * \brief Destructor.
     * \details
     * If `this->valid() == true`, returns the owned connection to the pool
     * and marks it as pending reset. If your connection doesn't need to be reset
     * (e.g. because you didn't mutate session state), use \ref return_without_reset.
     *
     * \par Thead-safety
     * If the \ref connection_pool object that `*this` references has been constructed
     * with adequate executor configuration, this function is safe to be called concurrently
     * with \ref connection_pool::async_run, \ref connection_pool::async_get_connection,
     * \ref connection_pool::cancel and \ref return_without_reset on other `pooled_connection` objects.
     */
    ~pooled_connection() = default;

    /**
     * \brief Returns whether the object owns a connection or not.
     * \par Exception safety
     * No-throw guarantee.
     */
    bool valid() const noexcept { return impl_.get() != nullptr; }

    /**
     * \brief Retrieves the connection owned by this object.
     * \par Preconditions
     * The object should own a connection (`this->valid() == true`).
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` or an object
     * move-constructed or move-assigned from `*this` is alive.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    any_connection& get() noexcept { return impl_->connection(); }

    /// \copydoc get
    const any_connection& get() const noexcept { return impl_->connection(); }

    /// \copydoc get
    any_connection* operator->() noexcept { return &get(); }

    /// \copydoc get
    const any_connection* operator->() const noexcept { return &get(); }

    /**
     * \brief Returns owned connection to the pool and skips reset.
     * \details
     * Returns a connection to the pool and marks it as iddle. This will
     * skip the \ref any_connection::async_reset_connection call to wipe session state.
     * \n
     * This can provide a performance gain, but must be used with care. Failing to wipe
     * session state can lead to resource leaks (prepared statements not being released)
     * incorrect results and vulnerabilities (different logical operations interacting due
     * to leaftover state).
     * \n
     * Please read the documentation on \ref any_connection::async_reset_connection before
     * calling this function. If in doubt, don't use it, and leave the destructor return
     * the connection to the pool for you.
     * \n
     * When this function returns, `*this` will own nothing (`this->valid() == false`).
     *
     * \par Preconditions
     * `this->valid() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Thead-safety
     * If the \ref connection_pool object that `*this` references has been constructed
     * with adequate executor configuration, this function is safe to be called concurrently
     * with \ref connection_pool::async_run, \ref connection_pool::async_get_connection,
     * \ref connection_pool::cancel and `~pooled_connection` (on other objects).
     */
    void return_without_reset() noexcept
    {
        BOOST_ASSERT(valid());
        impl_.release()->mark_as_collectable(false);
    }
};

}  // namespace mysql
}  // namespace boost

#endif
