#ifndef INCLUDE_MYSQL_IMPL_QUERY_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_QUERY_IMPL_HPP_

#include "mysql/impl/messages.hpp"
#include "mysql/impl/basic_serialization.hpp"
#include "mysql/impl/deserialize_row.hpp"
#include <boost/asio/yield.hpp>

namespace mysql
{
namespace detail
{


template <typename ChannelType, typename Allocator>
class query_processor
{
	ChannelType& channel_;
	bytestring<Allocator> buffer_;
	std::vector<field_metadata> fields_;
	std::vector<bytestring<Allocator>> field_buffers_;
public:
	query_processor(ChannelType& channel): channel_(channel) {};
	void process_query_request(
		std::string_view query
	)
	{
		// Compose a com_query message
		msgs::com_query query_msg;
		query_msg.query.value = query;

		// Serialize it
		capabilities caps = channel_.current_capabilities();
		serialize_message(query_msg, caps, buffer_);

		// Prepare the channel
		channel_.reset_sequence_number();
	}

	std::optional<std::uint64_t> // has value if there are fields in the response
	process_query_response(
		channel_resultset_type<ChannelType, Allocator>& output,
		error_code& err
	)
	{
		// Response may be: ok_packet, err_packet, local infile request (TODO)
		// If it is none of this, then the message type itself is the beginning of
		// a length-encoded int containing the field count
		DeserializationContext ctx (boost::asio::buffer(buffer_), channel_.current_capabilities());
		std::uint8_t msg_type;
		err = deserialize_message_type(msg_type, ctx);
		if (err) return {};
		if (msg_type == ok_packet_header)
		{
			msgs::ok_packet ok_packet;
			err = deserialize_message(ok_packet, ctx);
			if (err) return {};
			output = channel_resultset_type<ChannelType, Allocator>(channel_, std::move(buffer_), ok_packet);
			err.clear();
			return {};
		}
		else if (msg_type == error_packet_header)
		{
			err = process_error_packet(ctx);
			return {};
		}
		else
		{
			// Resultset with metadata. First packet is an int_lenenc with
			// the number of field definitions to expect. Message type is part
			// of this packet, so we must rewind the context
			ctx.rewind(1);
			int_lenenc num_fields;
			err = deserialize_message(num_fields, ctx);
			if (err) return {};

			fields_.reserve(num_fields.value);
			field_buffers_.reserve(num_fields.value);

			return num_fields.value;
		}
	}

	error_code process_field_definition()
	{
		msgs::column_definition field_definition;
		DeserializationContext ctx (boost::asio::buffer(buffer_), channel_.current_capabilities());
		auto err = deserialize_message(field_definition, ctx);
		if (err) return err;

		// Add it to our array
		fields_.push_back(field_definition);
		field_buffers_.push_back(std::move(buffer_));
		buffer_ = bytestring<Allocator>();

		return error_code();
	}

	void create_resultset(
		channel_resultset_type<ChannelType, Allocator>& output
	) &&
	{
		output = channel_resultset_type<ChannelType, Allocator>(
			channel_,
			resultset_metadata<Allocator>(std::move(field_buffers_), std::move(fields_))
		);
	}

	auto& channel() { return channel_; }
	auto& buffer() { return buffer_; }
};

} // detail
} // mysql

template <typename ChannelType, typename Allocator>
void mysql::detail::execute_query(
	ChannelType& channel,
	std::string_view query,
	resultset<channel_stream_type<ChannelType>, Allocator>& output,
	error_code& err
)
{
	// Compose a com_query message, reset seq num
	query_processor<ChannelType, Allocator> processor (channel);
	processor.process_query_request(query);

	// Send it
	channel.write(boost::asio::buffer(processor.buffer()), err);
	if (err) return;

	// Read the response
	channel.read(processor.buffer(), err);
	if (err) return;

	// Response may be: ok_packet, err_packet, local infile request (TODO), or response with fields
	auto num_fields = processor.process_query_response(output, err);
	if (!num_fields) // ok or err
	{
		return;
	}

	// We have a response with metadata, read all of the field definitions
	for (std::uint64_t i = 0; i < *num_fields; ++i)
	{
		// Read the field definition packet
		channel.read(processor.buffer(), err);
		if (err) return;

		// Process the message
		err = processor.process_field_definition();
		if (err) return;
	}

	// No EOF packet is expected here, as we require deprecate EOF capabilities
	std::move(processor).create_resultset(output);
	err.clear();
}


template <typename ChannelType, typename Allocator, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	void(mysql::error_code, mysql::detail::channel_resultset_type<ChannelType, Allocator>)
)
mysql::detail::async_execute_query(
	ChannelType& channel,
	std::string_view query,
	CompletionToken&& token
)
{
	using HandlerSignature = void(error_code, channel_resultset_type<ChannelType, Allocator>);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using StreamType = typename ChannelType::stream_type;
	using BaseType = boost::beast::async_base<HandlerType, typename StreamType::executor_type>;
	using ResultsetType = channel_resultset_type<ChannelType, Allocator>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op: BaseType, boost::asio::coroutine
	{
		std::shared_ptr<query_processor<ChannelType, Allocator>> processor_;
		std::uint64_t remaining_fields_ {0};

		Op(
			HandlerType&& handler,
			ChannelType& channel,
			std::string_view query
		):
			BaseType(std::move(handler), channel.next_layer().get_executor()),
			processor_(std::make_shared<query_processor<ChannelType, Allocator>>(channel))
		{
			processor_->process_query_request(query);
		}

		// true => has fields (must continue reading)
		bool process_query_response(bool cont)
		{
			ResultsetType resultset;
			error_code err;
			auto num_fields = processor_->process_query_response(resultset, err);
			if (!num_fields) // ok or err
			{
				this->complete(cont, err, std::move(resultset));
				return false;
			}
			else
			{
				remaining_fields_ = *num_fields;
				return true;
			}
		}

		void complete_with_fields(bool cont)
		{
			ResultsetType resultset;
			std::move(*processor_).create_resultset(resultset);
			this->complete(cont, error_code(), std::move(resultset));
		}

		void operator()(
			error_code err,
			bool cont=true
		)
		{
			reenter(*this)
			{
				// The request message has already been composed in the ctor. Send it
				yield processor_->channel().async_write(
					boost::asio::buffer(processor_->buffer()),
					std::move(*this)
				);
				if (err)
				{
					this->complete(cont, err, ResultsetType());
					yield break;
				}

				// Read the response
				yield processor_->channel().async_read(
					processor_->buffer(),
					std::move(*this)
				);
				if (err)
				{
					this->complete(cont, err, ResultsetType());
					yield break;
				}

				// Response may be: ok_packet, err_packet, local infile request (TODO), or response with fields
				if (!process_query_response(cont))
				{
					// Not a response with fields. complete() already called
					yield break;
				}

				// Read all of the field definitions
				while (remaining_fields_ > 0)
				{
					// Read the field definition packet
					yield processor_->channel().async_read(
						processor_->buffer(),
						std::move(*this)
					);
					if (!err)
					{
						// Process the message
						err = processor_->process_field_definition();
					}

					if (err)
					{
						this->complete(cont, err, ResultsetType());
						yield break;
					}

					remaining_fields_--;
				}

				// No EOF packet is expected here, as we require deprecate EOF capabilities
				complete_with_fields(cont);
				yield break;
			}
		}
	};

	Op(
		std::move(initiator.completion_handler),
		channel,
		query
	)(error_code(), false);
	return initiator.result.get();
}



template <typename ChannelType, typename Allocator>
mysql::detail::fetch_result mysql::detail::fetch_text_row(
	ChannelType& channel,
	const std::vector<field_metadata>& meta,
	bytestring<Allocator>& buffer,
	std::vector<value>& output_values,
	msgs::ok_packet& output_ok_packet,
	error_code& err
)
{
	// Read a packet
	channel.read(buffer, err);
	if (err) return fetch_result::error;

	// Message type: row, error or eof?
	std::uint8_t msg_type;
	DeserializationContext ctx (boost::asio::buffer(buffer), channel.current_capabilities());
	err = deserialize_message_type(msg_type, ctx);
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
		err = process_error_packet(ctx);
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

#include <boost/asio/unyield.hpp>


#endif /* INCLUDE_MYSQL_IMPL_QUERY_IMPL_HPP_ */
