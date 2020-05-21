//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CONNECT_HPP
#define BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CONNECT_HPP

#include "boost/mysql/detail/network_algorithms/handshake.hpp"

template <typename StreamType>
void boost::mysql::detail::connect(
    channel<StreamType>& chan,
    const typename StreamType::endpoint_type& endpoint,
    const connection_params& params,
    error_code& err,
    error_info& info
)
{
    chan.next_layer().connect(endpoint, err);
    if (err)
    {
        chan.close();
        info.set_message("Physical connect failed");
        return;
    }
    handshake(chan, params, err, info);
    if (err)
    {
        chan.close();
    }
}

namespace boost {
namespace mysql {
namespace detail {

template<class StreamType>
struct connect_op : async_op<StreamType>
{
  using endpoint_type = typename StreamType::endpoint_type;

  const endpoint_type& ep_; // No need for a copy, as we will call it in the first operator() call
  connection_params params_;

  connect_op(
      channel<StreamType>& chan,
      error_info* output_info,
      const endpoint_type& ep,
      const connection_params& params
  ) :
      async_op<StreamType>(chan, output_info),
      ep_(ep),
      params_(params)
  {
  }

  template<class Self>
  void operator()(
      Self& self,
      error_code code = {}
  )
  {
    BOOST_ASIO_CORO_REENTER(*this)
      {
        // Physical connect
        BOOST_ASIO_CORO_YIELD this->get_channel().next_layer().async_connect(
                          ep_,
                          std::move(self)
                      );
        if (code)
        {
          this->get_channel().close();
          if (this->get_output_info())
          {
            this->get_output_info()->set_message("Physical connect failed");
          }
          self.complete(code);
          BOOST_ASIO_CORO_YIELD break;
        }

        // Handshake
        BOOST_ASIO_CORO_YIELD async_handshake(
                          this->get_channel(),
                          params_,
                          std::move(self),
                          this->get_output_info()
                      );
        if (code)
        {
          this->get_channel().close();
        }
        self.complete(code);
      }
  }
};

}
}
}

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    boost::mysql::detail::connect_signature
)
boost::mysql::detail::async_connect(
    channel<StreamType>& chan,
    const typename StreamType::endpoint_type& endpoint,
    const connection_params& params,
    CompletionToken&& token,
    error_info* info
)
{
//    using endpoint_type = typename StreamType::endpoint_type;

    return boost::asio::async_compose<CompletionToken, connect_signature>(
        connect_op<StreamType>{chan, info, endpoint, params}, token, chan);

//    return op::initiate(std::forward<CompletionToken>(token), chan, info, endpoint, params);
}

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CONNECT_HPP_ */
