#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_PREPARE_STATEMENT_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_PREPARE_STATEMENT_HPP_

#include <boost/asio/yield.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <typename StreamType>
class prepare_statement_processor
{
	channel<StreamType>& channel_;
	com_stmt_prepare_ok_packet response_;
public:
	prepare_statement_processor(channel<StreamType>& chan): channel_(chan) {}
	void process_request(std::string_view statement)
	{
		com_stmt_prepare_packet packet { string_eof(statement) };
		serialize_message(packet, channel_.current_capabilities(), channel_.shared_buffer());
		channel_.reset_sequence_number();
	}
	void process_response(error_code& err, error_info& info)
	{
		deserialization_context ctx (
			boost::asio::buffer(channel_.shared_buffer()),
			channel_.current_capabilities()
		);
		std::uint8_t msg_type = 0;
		std::tie(err, msg_type) = deserialize_message_type(ctx);
		if (err) return;

		if (msg_type == error_packet_header)
		{
			err = process_error_packet(ctx, info);
		}
		else if (msg_type != 0)
		{
			err = make_error_code(errc::protocol_value_error);
		}
		else
		{
			err = deserialize_message(response_, ctx);
		}
	}
	auto& get_buffer() noexcept { return channel_.shared_buffer(); }
	auto& get_channel() noexcept { return channel_; }
	const auto& get_response() const noexcept { return response_; }

	unsigned get_num_metadata_packets() const noexcept
	{
		return response_.num_columns.value + response_.num_params.value;
	}
};

} // detail
} // mysql
} // boost

template <typename StreamType>
void boost::mysql::detail::prepare_statement(
	channel<StreamType>& channel,
	std::string_view statement,
	error_code& err,
	error_info& info,
	prepared_statement<StreamType>& output
)
{
	// Prepare message
	prepare_statement_processor<StreamType> processor (channel);
	processor.process_request(statement);

	// Write message
	processor.get_channel().write(boost::asio::buffer(processor.get_buffer()), err);
	if (err) return;

	// Read response
	processor.get_channel().read(processor.get_buffer(), err);
	if (err) return;

	// Process response
	processor.process_response(err, info);
	if (err) return;

	// Server sends now one packet per parameter and field.
	// We ignore these for now. TODO: do sth useful with these
	for (unsigned i = 0; i < processor.get_num_metadata_packets(); ++i)
	{
		processor.get_channel().read(processor.get_buffer(), err);
		if (err) return;
	}

	// Compose response
	output = prepared_statement<StreamType>(channel, processor.get_response());
}

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	boost::mysql::detail::prepare_statement_signature<StreamType>
)
boost::mysql::detail::async_prepare_statement(
	channel<StreamType>& chan,
	std::string_view statement,
	CompletionToken&& token,
	error_info* info
)
{
	using HandlerSignature = prepare_statement_signature<StreamType>;
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using BaseType = boost::beast::async_base<HandlerType, typename StreamType::executor_type>;
	using PreparedStatementType = prepared_statement<StreamType>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op: BaseType, boost::asio::coroutine
	{
		prepare_statement_processor<StreamType> processor_;
		unsigned remaining_meta_;
		error_info* output_info_;

		Op(
			HandlerType&& handler,
			channel<StreamType>& channel,
			std::string_view statement,
			error_info* output_info
		):
			BaseType(std::move(handler), channel.next_layer().get_executor()),
			processor_(channel),
			remaining_meta_(0),
			output_info_(output_info)
		{
			processor_.process_request(statement);
		}

		void operator()(
			error_code err,
			bool cont=true
		)
		{
			// Error checking
			if (err)
			{
				this->complete(cont, err, PreparedStatementType());
				return;
			}

			// Regular coroutine body; if there has been an error, we don't get here
			error_info info;
			reenter(*this)
			{
				// Write message (already serialized at this point)
				yield processor_.get_channel().async_write(
					boost::asio::buffer(processor_.get_buffer()),
					std::move(*this)
				);

				// Read response
				yield processor_.get_channel().async_read(
					processor_.get_buffer(),
					std::move(*this)
				);

				// Process response
				processor_.process_response(err, info);
				if (err)
				{
					detail::conditional_assign(output_info_, std::move(info));
					this->complete(cont, err, PreparedStatementType());
					yield break;
				}

				// Server sends now one packet per parameter and field.
				// We ignore these for now. TODO: do sth useful with these
				remaining_meta_ = processor_.get_num_metadata_packets();
				for (; remaining_meta_ > 0; --remaining_meta_)
				{
					yield processor_.get_channel().async_read(
						processor_.get_buffer(),
						std::move(*this)
					);
				}

				// Compose response
				this->complete(
					cont,
					err,
					PreparedStatementType(processor_.get_channel(), processor_.get_response())
				);
			}
		}
	};

	Op(
		std::move(initiator.completion_handler),
		chan,
		statement,
		info
	)(error_code(), false);
	return initiator.result.get();
}

#include <boost/asio/unyield.hpp>

#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_PREPARE_STATEMENT_HPP_ */
