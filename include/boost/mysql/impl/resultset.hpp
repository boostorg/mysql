//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_RESULTSET_HPP
#define BOOST_MYSQL_IMPL_RESULTSET_HPP

#include "boost/mysql/detail/network_algorithms/read_row.hpp"
#include "boost/mysql/detail/auxiliar/check_completion_token.hpp"
#include <boost/asio/coroutine.hpp>
#include <cassert>
#include <limits>
#include <boost/asio/yield.hpp>

template <typename StreamType>
const boost::mysql::row* boost::mysql::resultset<StreamType>::fetch_one(
    error_code& err,
    error_info& info
)
{
    assert(valid());

    detail::clear_errors(err, info);

    if (complete())
    {
        return nullptr;
    }
    auto result = detail::read_row(
        deserializer_,
        *channel_,
        meta_.fields(),
        buffer_,
        current_row_.values(),
        ok_packet_,
        err,
        info
    );
    eof_received_ = result == detail::read_row_result::eof;
    return result == detail::read_row_result::row ? &current_row_ : nullptr;
}

template <typename StreamType>
const boost::mysql::row* boost::mysql::resultset<StreamType>::fetch_one()
{
    error_code code;
    error_info info;
    const row* res = fetch_one(code, info);
    detail::check_error_code(code, info);
    return res;
}

template <typename StreamType>
std::vector<boost::mysql::owning_row> boost::mysql::resultset<StreamType>::fetch_many(
    std::size_t count,
    error_code& err,
    error_info& info
)
{
    assert(valid());

    detail::clear_errors(err, info);

    std::vector<mysql::owning_row> res;

    if (!complete()) // support calling fetch on already exhausted resultset
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            detail::bytestring buff;
            std::vector<value> values;

            auto result = detail::read_row(
                deserializer_,
                *channel_,
                meta_.fields(),
                buff,
                values,
                ok_packet_,
                err,
                info
            );
            eof_received_ = result == detail::read_row_result::eof;
            if (result == detail::read_row_result::row)
            {
                res.emplace_back(std::move(values), std::move(buff));
            }
            else
            {
                break;
            }
        }
    }

    return res;
}

template <typename StreamType>
std::vector<boost::mysql::owning_row> boost::mysql::resultset<StreamType>::fetch_many(
    std::size_t count
)
{
    error_code code;
    error_info info;
    auto res = fetch_many(count, code, info);
    detail::check_error_code(code, info);
    return res;
}

template <typename StreamType>
std::vector<boost::mysql::owning_row> boost::mysql::resultset<StreamType>::fetch_all(
    error_code& err,
    error_info& info
)
{
    return fetch_many(std::numeric_limits<std::size_t>::max(), err, info);
}

template <typename StreamType>
std::vector<boost::mysql::owning_row> boost::mysql::resultset<StreamType>::fetch_all()
{
    return fetch_many(std::numeric_limits<std::size_t>::max());
}

template <typename StreamType>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::resultset<StreamType>::fetch_one_signature
)
boost::mysql::resultset<StreamType>::async_fetch_one(
    CompletionToken&& token,
    error_info* info
)
{
    assert(valid());
    detail::conditional_clear(info);
    detail::check_completion_token<CompletionToken, fetch_one_signature>();

    struct op: detail::async_op<StreamType, CompletionToken, fetch_one_signature, op>
    {
        resultset<StreamType>& resultset_;

        op(
            boost::asio::async_completion<CompletionToken, fetch_one_signature>& completion,
            detail::channel<StreamType>& chan,
            error_info* output_info,
            resultset<StreamType>& obj
        ):
            detail::async_op<StreamType, CompletionToken, fetch_one_signature, op>(completion, chan, output_info),
            resultset_(obj)
        {
        };

        void operator()(
            error_code err,
            error_info info={},
            detail::read_row_result result=detail::read_row_result::error,
            bool cont=true
        )
        {
            reenter(*this)
            {
                if (resultset_.complete())
                {
                    this->complete(cont, error_code(), nullptr);
                    yield break;
                }
                yield detail::async_read_row(
                    resultset_.deserializer_,
                    *resultset_.channel_,
                    resultset_.meta_.fields(),
                    resultset_.buffer_,
                    resultset_.current_row_.values(),
                    resultset_.ok_packet_,
                    std::move(*this)
                );
                resultset_.eof_received_ = result == detail::read_row_result::eof;
                detail::conditional_assign(this->get_output_info(), std::move(info));
                this->complete(
                    cont,
                    err,
                    result == detail::read_row_result::row ? &resultset_.current_row_ : nullptr
                );
            }
        }
    };

    return op::initiate(std::forward<CompletionToken>(token), *channel_, info, *this);
}

template <typename StreamType>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::resultset<StreamType>::fetch_many_signature
)
boost::mysql::resultset<StreamType>::async_fetch_many(
    std::size_t count,
    CompletionToken&& token,
    error_info* info
)
{
    assert(valid());
    detail::conditional_clear(info);
    detail::check_completion_token<CompletionToken, fetch_many_signature>();

    struct op_impl
    {
        resultset<StreamType>& parent_resultset;
        std::vector<owning_row> rows;
        detail::bytestring buffer;
        std::vector<value> values;
        std::size_t remaining;

        op_impl(resultset<StreamType>& obj, std::size_t count):
            parent_resultset(obj),
            remaining(count)
        {
        };

        void row_received()
        {
            rows.emplace_back(std::move(values), std::move(buffer));
            values = std::vector<value>();
            buffer = detail::bytestring();
            --remaining;
        }
    };

    struct op : detail::async_op<StreamType, CompletionToken, fetch_many_signature, op>
    {
        std::shared_ptr<op_impl> impl_;

        op(
            boost::asio::async_completion<CompletionToken, fetch_many_signature>& completion,
            detail::channel<StreamType>& chan,
            error_info* output_info,
            resultset<StreamType>& obj,
            std::size_t count
        ) :
            detail::async_op<StreamType, CompletionToken, fetch_many_signature, op>(
                    completion, chan, output_info),
            impl_(std::make_shared<op_impl>(obj, count))
        {
        };

        void operator()(
            error_code err,
            error_info info={},
            detail::read_row_result result=detail::read_row_result::error,
            bool cont=true
        )
        {
            reenter(*this)
            {
                while (!impl_->parent_resultset.complete() && impl_->remaining > 0)
                {
                    yield detail::async_read_row(
                        impl_->parent_resultset.deserializer_,
                        this->get_channel(),
                        impl_->parent_resultset.meta_.fields(),
                        impl_->buffer,
                        impl_->values,
                        impl_->parent_resultset.ok_packet_,
                        std::move(*this)
                    );
                    if (result == detail::read_row_result::error)
                    {
                        detail::conditional_assign(this->get_output_info(), std::move(info));
                        this->complete(cont, err, std::move(impl_->rows));
                        yield break;
                    }
                    else if (result == detail::read_row_result::eof)
                    {
                        impl_->parent_resultset.eof_received_ = true;
                    }
                    else
                    {
                        impl_->row_received();
                    }
                }
                this->complete(cont, err, std::move(impl_->rows));
            }
        }
    };

    return op::initiate(std::forward<CompletionToken>(token), *channel_, info, *this, count);
}

template <typename StreamType>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    typename boost::mysql::resultset<StreamType>::fetch_all_signature
)
boost::mysql::resultset<StreamType>::async_fetch_all(
    CompletionToken&& token,
    error_info* info
)
{
    return async_fetch_many(
        std::numeric_limits<std::size_t>::max(),
        std::forward<CompletionToken>(token),
        info
    );
}


#include <boost/asio/unyield.hpp>


#endif
