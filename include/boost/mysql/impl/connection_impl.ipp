//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_IMPL_IPP
#define BOOST_MYSQL_IMPL_CONNECTION_IMPL_IPP

#pragma once

#include <boost/mysql/detail/any_stream.hpp>
#include <boost/mysql/detail/connection_impl.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state.hpp>

#include <memory>

std::vector<boost::mysql::field_view>& boost::mysql::detail::connection_impl::get_shared_fields(
    connection_state& st
) noexcept
{
    return st.data().shared_fields;
}

boost::mysql::detail::connection_impl::connection_impl(
    std::size_t read_buff_size,
    std::unique_ptr<any_stream> stream
)
    : stream_(std::move(stream)), st_(new connection_state(read_buff_size, stream_->supports_ssl()))
{
}

boost::mysql::detail::connection_impl::connection_impl(connection_impl&& rhs) noexcept
    : stream_(std::move(rhs.stream_)), st_(std::move(rhs.st_))
{
}

boost::mysql::detail::connection_impl& boost::mysql::detail::connection_impl::operator=(connection_impl&& rhs
) noexcept
{
    stream_ = std::move(rhs.stream_);
    st_ = std::move(rhs.st_);
    return *this;
}

boost::mysql::detail::connection_impl::~connection_impl() {}

boost::mysql::metadata_mode boost::mysql::detail::connection_impl::meta_mode() const noexcept
{
    return st_->data().meta_mode;
}

void boost::mysql::detail::connection_impl::set_meta_mode(metadata_mode v) noexcept
{
    st_->data().meta_mode = v;
}

bool boost::mysql::detail::connection_impl::ssl_active() const noexcept { return st_->data().ssl_active(); }

boost::mysql::diagnostics& boost::mysql::detail::connection_impl::shared_diag() noexcept
{
    return st_->data().shared_diag;
}

#endif
