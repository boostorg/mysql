//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_RESULTSET_HPP
#define BOOST_MYSQL_IMPL_RESULTSET_HPP

#include "boost/mysql/detail/network_algorithms/read_row.hpp"
#include <boost/asio/coroutine.hpp>
#include <boost/asio/bind_executor.hpp>
#include <cassert>
#include <limits>

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
    detail::error_block blk;
    const row* res = fetch_one(blk.err, blk.info);
    blk.check();
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
    detail::error_block blk;
    auto res = fetch_many(count, blk.err, blk.info);
    blk.check();
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

template<typename StreamType>
struct boost::mysql::resultset<StreamType>::fetch_one_op
    : boost::asio::coroutine
{
    resultset<StreamType>& resultset_;
    error_info* output_info_;

    fetch_one_op(
        resultset<StreamType>& obj,
        error_info* output_info
    ) :
        resultset_(obj),
        output_info_(output_info)
    {
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code err = {},
        detail::read_row_result result=detail::read_row_result::error
    )
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            if (resultset_.complete())
            {
                // ensure return as if by post
                BOOST_ASIO_CORO_YIELD boost::asio::post(std::move(self));
                self.complete(error_code(), nullptr);
                BOOST_ASIO_CORO_YIELD break;
            }
            BOOST_ASIO_CORO_YIELD detail::async_read_row(
                resultset_.deserializer_,
                *resultset_.channel_,
                resultset_.meta_.fields(),
                resultset_.buffer_,
                resultset_.current_row_.values(),
                resultset_.ok_packet_,
                std::move(self),
                output_info_
            );
            resultset_.eof_received_ = result == detail::read_row_result::eof;
            self.complete(
                err,
                result == detail::read_row_result::row ? &resultset_.current_row_ : nullptr
            );
        }
    }
};

template <typename StreamType>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    typename boost::mysql::resultset<StreamType>::fetch_one_signature) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
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
    return boost::asio::async_compose<CompletionToken, fetch_one_signature>(
        fetch_one_op(*this, info),
        token,
        *this
    );
}



template<typename StreamType>
struct boost::mysql::resultset<StreamType>::fetch_many_op
    : boost::asio::coroutine
{
    struct impl_struct
    {
        resultset<StreamType>& parent_resultset;
        std::vector<owning_row> rows;
        detail::bytestring buffer;
        std::vector<value> values;
        std::size_t remaining;
        error_info* output_info;
        bool cont {false};

        impl_struct(resultset<StreamType>& obj, error_info* output_info, std::size_t count):
            parent_resultset(obj),
            remaining(count),
            output_info(output_info)
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

    std::shared_ptr<impl_struct> impl_;

    fetch_many_op(
        resultset<StreamType>& obj,
        error_info* output_info,
        std::size_t count
    ) :
        impl_(std::make_shared<impl_struct>(obj, output_info, count))
    {
    };

    template <typename Self>
    auto bind_handler(Self& self, error_code err)
    {
        auto executor = boost::asio::get_associated_executor(
            self,
            impl_->parent_resultset.get_executor()
        );
        return boost::asio::bind_executor(
            executor,
            std::bind(std::move(self), err, detail::read_row_result::eof)
        );
    }

    template<class Self>
    void operator()(
        Self& self,
        error_code err = {},
        detail::read_row_result result=detail::read_row_result::error
    )
    {
        impl_struct& impl = *impl_;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            while (!impl.parent_resultset.complete() && impl.remaining > 0)
            {
                impl.cont = true;
                BOOST_ASIO_CORO_YIELD detail::async_read_row(
                    impl.parent_resultset.deserializer_,
                    *impl.parent_resultset.channel_,
                    impl.parent_resultset.meta_.fields(),
                    impl.buffer,
                    impl.values,
                    impl.parent_resultset.ok_packet_,
                    std::move(self),
                    impl.output_info
                );
                if (result == detail::read_row_result::error)
                {
                    self.complete(err, std::move(impl.rows));
                    BOOST_ASIO_CORO_YIELD break;
                }
                else if (result == detail::read_row_result::eof)
                {
                    impl.parent_resultset.eof_received_ = true;
                }
                else
                {
                    impl.row_received();
                }
            }

            if (!impl.cont)
            {
                // Ensure we call handler as if dispatched using post
                // through the correct executor
                BOOST_ASIO_CORO_YIELD
                boost::asio::post(bind_handler(self, err));
            }

            self.complete(err, std::move(impl.rows));
        }
    }
};

template <typename StreamType>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    typename boost::mysql::resultset<StreamType>::fetch_many_signature) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
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
    return boost::asio::async_compose<CompletionToken, fetch_many_signature>(
        fetch_many_op(*this, info, count),
        token,
        *this
    );
}

template <typename StreamType>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    typename boost::mysql::resultset<StreamType>::fetch_all_signature) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
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


#endif
