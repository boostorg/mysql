//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_COMMON_TEST_STREAM_HPP
#define BOOST_MYSQL_TEST_COMMON_TEST_STREAM_HPP

#include <boost/asio/executor.hpp>
#include <boost/mysql/error.hpp>

namespace boost {
namespace mysql {
namespace test {

class test_stream
{
public:
    using executor_type = boost::asio::executor;

    using lowest_layer_type = test_stream;
    
    lowest_layer_type& lowest_layer() noexcept { return *this; }

    executor_type get_executor() noexcept { return executor_type(); }

    template<class MutableBufferSequence>
    std::size_t
    read_some(const MutableBufferSequence&) { return 0; }

    template<class MutableBufferSequence>
    std::size_t
    read_some(const MutableBufferSequence&, boost::mysql::error_code& ec) { ec = {}; return 0; }

    template<
        class MutableBufferSequence,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, std::size_t)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, std::size_t))
    async_read_some(
        const MutableBufferSequence&,
        CompletionToken&& BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
    }

    template<class ConstBufferSequence>
    std::size_t
    write_some(const ConstBufferSequence&) { return 0; }

    template<class ConstBufferSequence>
    std::size_t
    write_some(const ConstBufferSequence&, error_code& ec) { ec = {}; return 0; }

    template<
        class ConstBufferSequence,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, std::size_t)) CompletionToken
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)
    >
    BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code, std::size_t))
    async_write_some(
        const ConstBufferSequence&,
        CompletionToken&& BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)
    )
    {
    }
};

}
}
}

#endif