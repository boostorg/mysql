#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_

#include "mysql/impl/text_deserialization.hpp"
#include <boost/asio/yield.hpp>

namespace mysql
{
namespace detail
{

inline fetch_result process_fetch_message(
	capabilities current_capabilities,
	const std::vector<field_metadata>& meta,
	const bytestring& buffer,
	std::vector<value>& output_values,
	ok_packet& output_ok_packet,
	error_code& err,
	error_info& info
)
{
	// Message type: row, error or eof?
	std::uint8_t msg_type;
	DeserializationContext ctx (boost::asio::buffer(buffer), current_capabilities);
	std::tie(err, msg_type) = deserialize_message_type(ctx);
	if (err) return fetch_result::error;
	if (msg_type == eof_packet_header)
	{
		// end of resultset
		err = deserialize_message(output_ok_packet, ctx);
		if (err) return fetch_result::error;
		return fetch_result::eof;
	}
	else if (msg_type == error_packet_header)
	{
		// An error occurred during the generation of the rows
		err = process_error_packet(ctx, info);
		return fetch_result::error;
	}
	else
	{
		// An actual row
		ctx.rewind(1); // keep the 'message type' byte, as it is part of the actual message
		err = deserialize_text_row(ctx, meta, output_values);
		if (err) return fetch_result::error;
		return fetch_result::row;
	}
}

} // detail
} // mysql



template <typename ChannelType>
mysql::detail::fetch_result mysql::detail::fetch_text_row(
	ChannelType& channel,
	const std::vector<field_metadata>& meta,
	bytestring& buffer,
	std::vector<value>& output_values,
	ok_packet& output_ok_packet,
	error_code& err,
	error_info& info
)
{
	// Read a packet
	channel.read(buffer, err);
	if (err) return fetch_result::error;

	return process_fetch_message(
		channel.current_capabilities(),
		meta,
		buffer,
		output_values,
		output_ok_packet,
		err,
		info
	);
}


template <typename ChannelType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	void(mysql::error_code, mysql::error_info, mysql::detail::fetch_result)
)
mysql::detail::async_fetch_text_row(
	ChannelType& channel,
	const std::vector<field_metadata>& meta,
	bytestring& buffer,
	std::vector<value>& output_values,
	ok_packet& output_ok_packet,
	CompletionToken&& token
)
{
	using HandlerSignature = void(mysql::error_code, mysql::detail::fetch_result);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using StreamType = channel_stream_type<ChannelType>;
	using BaseType = boost::beast::async_base<HandlerType, typename StreamType::executor_type>;

	struct Op: BaseType, boost::asio::coroutine
	{
		ChannelType& channel_;
		const std::vector<field_metadata>& meta_;
		bytestring& buffer_;
		std::vector<value>& output_values_;
		ok_packet& output_ok_packet_;

		Op(
			HandlerType&& handler,
			ChannelType& channel,
			const std::vector<field_metadata>& meta,
			bytestring& buffer,
			std::vector<value>& output_values,
			ok_packet& output_ok_packet
		):
			BaseType(std::move(handler), channel.next_layer().get_executor()),
			channel_(channel),
			meta_(meta),
			buffer_(buffer),
			output_values_(output_values),
			output_ok_packet_(output_ok_packet)
		{
		}

		void process_result(bool cont)
		{
			error_code err;
			error_info info;
			auto result = process_fetch_message(
				channel_.current_capabilities(),
				meta_,
				buffer_,
				output_values_,
				output_ok_packet_,
				err,
				info
			);
			this->complete(cont, err, info, result);
		}

		void operator()(
			error_code err,
			bool cont=true
		)
		{
			reenter(*this)
			{
				yield channel_.async_read(buffer_, std::move(*this));
				if (err)
				{
					this->complete(cont, err, error_info(), fetch_result::error);
					yield break;
				}
				process_result(cont);
			}
		}
	};

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	Op(
		std::move(initiator.completion_handler),
		channel,
		meta,
		buffer,
		output_values,
		output_ok_packet
	)(error_code(), false);
	return initiator.result.get();
}

#include <boost/asio/unyield.hpp>

#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_READ_TEXT_ROW_IPP_ */
