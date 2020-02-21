#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_IPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_IPP_

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/async_result.hpp>

template <typename StreamType>
void mysql::detail::close_statement(
	channel<StreamType>& chan,
	std::uint32_t statement_id,
	error_code& errc,
	error_info&
)
{
	// Compose the close message
	com_stmt_close_packet packet {int4(statement_id)};

	// Serialize it
	serialize_message(packet, chan.current_capabilities(), chan.shared_buffer());

	// Send it. No response is sent back
	chan.reset_sequence_number();
	chan.write(boost::asio::buffer(chan.shared_buffer()), errc);
}

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(mysql::error_code, mysql::error_info))
mysql::detail::async_close_statement(
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

	return boost::asio::async_initiate<CompletionToken, void(error_code, error_info)>(
			initiation, token, chan);
}


#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_CLOSE_STATEMENT_IPP_ */
