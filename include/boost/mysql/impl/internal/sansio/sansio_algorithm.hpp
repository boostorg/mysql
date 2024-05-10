//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_SANSIO_ALGORITHM_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_SANSIO_ALGORITHM_HPP

#include <boost/mysql/error_code.hpp>

#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

namespace boost {
namespace mysql {
namespace detail {

class sansio_algorithm
{
protected:
    connection_state_data* st_;

    next_action read(std::uint8_t& seqnum, bool keep_parsing_state = false)
    {
        // buffer is attached by top_level_algo
        st_->reader.prepare_read(seqnum, keep_parsing_state);
        return next_action::read(next_action::read_args_t{{}, false});
    }

    template <class Serializable>
    next_action write(const Serializable& msg, std::uint8_t& seqnum)
    {
        // buffer is attached by top_level_algo
        st_->writer.prepare_write(msg, seqnum);
        return next_action::write(next_action::write_args_t{{}, false});
    }

    sansio_algorithm(connection_state_data& st) noexcept : st_(&st) {}

public:
    const connection_state_data& conn_state() const noexcept { return *st_; }
    connection_state_data& conn_state() noexcept { return *st_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
