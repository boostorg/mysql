//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CHANNEL_IMPL_CHANNEL_PTR_IPP
#define BOOST_MYSQL_DETAIL_CHANNEL_IMPL_CHANNEL_PTR_IPP

#pragma once

#include <boost/mysql/detail/channel/channel.hpp>
#include <boost/mysql/detail/channel/channel_ptr.hpp>

boost::mysql::detail::channel_ptr::channel_ptr(std::size_t read_buff_size, std::unique_ptr<any_stream> stream)
    : chan_(new channel(read_buff_size, std::move(stream)))
{
}

boost::mysql::detail::channel_ptr::channel_ptr(channel_ptr&& rhs) noexcept : chan_(std::move(rhs.chan_)) {}

boost::mysql::detail::channel_ptr& boost::mysql::detail::channel_ptr::operator=(channel_ptr&& rhs) noexcept
{
    chan_ = std::move(rhs.chan_);
    return *this;
}

boost::mysql::detail::channel_ptr::~channel_ptr() {}

const boost::mysql::detail::any_stream& boost::mysql::detail::channel_ptr::get_stream() const
{
    return chan_->stream();
}

boost::mysql::metadata_mode boost::mysql::detail::channel_ptr::meta_mode() const noexcept
{
    return chan_->meta_mode();
}

void boost::mysql::detail::channel_ptr::set_meta_mode(metadata_mode v) noexcept { chan_->set_meta_mode(v); }

#endif
