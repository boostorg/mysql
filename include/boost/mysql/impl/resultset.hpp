//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
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
#include <memory>

template <class Stream>
bool boost::mysql::resultset<Stream>::read_one(
	row& output,
    error_code& err,
    error_info& info
)
{
    assert(valid());

    detail::clear_errors(err, info);

    if (complete())
    {
        output.clear();
        return false;
    }
    auto result = detail::read_row(
        deserializer_,
        *channel_,
        meta_.fields(),
        output,
		ok_packet_buffer_,
        ok_packet_,
        err,
        info
    );
    eof_received_ = result == detail::read_row_result::eof;
    return result == detail::read_row_result::row;
}

template <class Stream>
bool boost::mysql::resultset<Stream>::read_one(
	row& output
)
{
    detail::error_block blk;
    bool res = read_one(output, blk.err, blk.info);
    blk.check();
    return res;
}

template <class Stream>
std::vector<boost::mysql::row> boost::mysql::resultset<Stream>::read_many(
    std::size_t count,
    error_code& err,
    error_info& info
)
{
    assert(valid());

    detail::clear_errors(err, info);

    std::vector<row> res;

	for (std::size_t i = 0; i < count; ++i)
	{
		row r;
		if (this->read_one(r, err, info))
		{
			res.push_back(std::move(r));
		}
		else
		{
			break;
		}
	}

    return res;
}

template <class Stream>
std::vector<boost::mysql::row> boost::mysql::resultset<Stream>::read_many(
    std::size_t count
)
{
    detail::error_block blk;
    auto res = read_many(count, blk.err, blk.info);
    blk.check();
    return res;
}

template <class Stream>
std::vector<boost::mysql::row> boost::mysql::resultset<Stream>::read_all(
    error_code& err,
    error_info& info
)
{
    return read_many(std::numeric_limits<std::size_t>::max(), err, info);
}

template <class Stream>
std::vector<boost::mysql::row> boost::mysql::resultset<Stream>::read_all()
{
    return read_many(std::numeric_limits<std::size_t>::max());
}

template<class Stream>
struct boost::mysql::resultset<Stream>::read_one_op
    : boost::asio::coroutine
{
    resultset<Stream>& resultset_;
    row& output_;
    error_info& output_info_;

    read_one_op(
        resultset<Stream>& obj,
		row& output,
        error_info& output_info
    ) :
        resultset_(obj),
		output_(output),
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
                output_.clear();
                self.complete(error_code(), false);
                BOOST_ASIO_CORO_YIELD break;
            }
            BOOST_ASIO_CORO_YIELD detail::async_read_row(
                resultset_.deserializer_,
                *resultset_.channel_,
                resultset_.meta_.fields(),
				output_,
                resultset_.ok_packet_buffer_,
                resultset_.ok_packet_,
                std::move(self),
                output_info_
            );
            resultset_.eof_received_ = result == detail::read_row_result::eof;
            self.complete(
                err,
                result == detail::read_row_result::row
            );
        }
    }
};

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(boost::mysql::error_code, bool)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, bool)
)
boost::mysql::resultset<Stream>::async_read_one(
	row& output,
    error_info& output_info,
    CompletionToken&& token
)
{
    assert(valid());
    output_info.clear();
    return boost::asio::async_compose<CompletionToken, void(error_code, bool)>(
        read_one_op(*this, output, output_info),
        token,
        *this
    );
}



template<class Stream>
struct boost::mysql::resultset<Stream>::read_many_op
    : boost::asio::coroutine
{
    struct impl_struct
    {
        resultset<Stream>& parent_resultset;
        std::vector<row> rows;
        row current_row;
        std::size_t remaining;
        error_info& output_info;
        bool cont {false};

        impl_struct(resultset<Stream>& obj, error_info& output_info, std::size_t count):
            parent_resultset(obj),
            remaining(count),
            output_info(output_info)
        {
        };

        void row_received()
        {
            rows.push_back(std::move(current_row));
            current_row = row();
            --remaining;
        }
    };

    std::shared_ptr<impl_struct> impl_;

    read_many_op(
        resultset<Stream>& obj,
        error_info& output_info,
        std::size_t count
    ) :
        impl_(std::make_shared<impl_struct>(obj, output_info, count))
    {
    };

    template <class Self>
    auto bind_handler(Self& self, error_code err) ->
        boost::asio::executor_binder<
            decltype(std::bind(std::move(self), err, detail::read_row_result::eof)),
            decltype(boost::asio::get_associated_executor(self, impl_->parent_resultset.get_executor()))
        >
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
        detail::read_row_result result = detail::read_row_result::error
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
					impl.current_row,
					impl.parent_resultset.ok_packet_buffer_,
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

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(boost::mysql::error_code, std::vector<boost::mysql::row>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, std::vector<boost::mysql::row>)
)
boost::mysql::resultset<Stream>::async_read_many(
    std::size_t count,
    error_info& output_info,
    CompletionToken&& token
)
{
    assert(valid());
    output_info.clear();
    return boost::asio::async_compose<
        CompletionToken,
        void(error_code, std::vector<row>)
    >(
        read_many_op(*this, output_info, count),
        token,
        *this
    );
}

template <class Stream>
template <BOOST_ASIO_COMPLETION_TOKEN_FOR(
    void(boost::mysql::error_code, std::vector<boost::mysql::row>)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(boost::mysql::error_code, std::vector<boost::mysql::row>)
)
boost::mysql::resultset<Stream>::async_read_all(
    error_info& info,
    CompletionToken&& token
)
{
    return async_read_many(
        std::numeric_limits<std::size_t>::max(),
        info,
        std::forward<CompletionToken>(token)
    );
}


#endif
