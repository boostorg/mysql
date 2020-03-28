#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP_

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/async_result.hpp>
#include "boost/mysql/detail/protocol/prepared_statement_messages.hpp"

template <typename StreamType>
void boost::mysql::detail::close_statement(
	channel<StreamType>& chan,
	std::uint32_t statement_id,
	error_code& code,
	error_info&
)
{
	// Compose the close message
	com_stmt_close_packet packet {int4(statement_id)};

	// Serialize it
	serialize_message(packet, chan.current_capabilities(), chan.shared_buffer());

	// Send it. No response is sent back
	chan.reset_sequence_number();
	chan.write(boost::asio::buffer(chan.shared_buffer()), code);
}

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	boost::mysql::detail::close_signature
)
boost::mysql::detail::async_close_statement(
	channel<StreamType>& chan,
	std::uint32_t statement_id,
	CompletionToken&& token
)
{
	// Compose the close message
	com_stmt_close_packet packet {int4(statement_id)};

	// Serialize it
	serialize_message(packet, chan.current_capabilities(), chan.shared_buffer());

	// Send it. No response is sent back
	chan.reset_sequence_number();

	auto initiation = [](
		auto&& completion_handler,
		channel<StreamType>& chan
	)
	{
		// Executor to use
		auto executor = boost::asio::get_associated_executor(
			completion_handler,
			chan.next_layer().get_executor()
		);

		// Preserve the executor to use
		chan.async_write(
			boost::asio::buffer(chan.shared_buffer()),
			boost::asio::bind_executor(
				executor,
				std::bind(
					std::forward<decltype(completion_handler)>(completion_handler),
					std::placeholders::_1,
					error_info()
				)
			)
		);
	};

	return boost::asio::async_initiate<CompletionToken, close_signature>(
			initiation, token, chan);
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_STATEMENT_HPP_ */
