#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_QUERY_IPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_QUERY_IPP_

#include "mysql/impl/messages.hpp"
#include "mysql/impl/serialization.hpp"
#include <optional>
#include <boost/asio/yield.hpp>

namespace mysql
{
namespace detail
{


template <typename StreamType>
class query_processor
{
	channel<StreamType>& channel_;
	bytestring buffer_;
	std::vector<field_metadata> fields_;
	std::vector<bytestring> field_buffers_;
public:
	query_processor(channel<StreamType>& chan): channel_(chan) {};
	void process_query_request(
		std::string_view query
	)
	{
		// Compose a com_query message
		com_query_packet query_msg;
		query_msg.query.value = query;

		// Serialize it
		capabilities caps = channel_.current_capabilities();
		serialize_message(query_msg, caps, buffer_);

		// Prepare the channel
		channel_.reset_sequence_number();
	}

	std::optional<std::uint64_t> // has value if there are fields in the response
	process_query_response(
		resultset<StreamType>& output,
		error_code& err,
		error_info& info
	)
	{
		// Response may be: ok_packet, err_packet, local infile request (not implemented)
		// If it is none of this, then the message type itself is the beginning of
		// a length-encoded int containing the field count
		DeserializationContext ctx (boost::asio::buffer(buffer_), channel_.current_capabilities());
		std::uint8_t msg_type;
		std::tie(err, msg_type) = deserialize_message_type(ctx);
		if (err) return {};
		if (msg_type == ok_packet_header)
		{
			ok_packet ok_packet;
			err = deserialize_message(ok_packet, ctx);
			if (err) return {};
			output = resultset<StreamType>(channel_, std::move(buffer_), ok_packet);
			err.clear();
			return {};
		}
		else if (msg_type == error_packet_header)
		{
			err = process_error_packet(ctx, info);
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
		column_definition_packet field_definition;
		DeserializationContext ctx (boost::asio::buffer(buffer_), channel_.current_capabilities());
		auto err = deserialize_message(field_definition, ctx);
		if (err) return err;

		// Add it to our array
		fields_.push_back(field_definition);
		field_buffers_.push_back(std::move(buffer_));
		buffer_ = bytestring();

		return error_code();
	}

	resultset<StreamType> create_resultset() &&
	{
		return resultset<StreamType>(
			channel_,
			resultset_metadata(std::move(field_buffers_), std::move(fields_))
		);
	}

	auto& get_channel() { return channel_; }
	auto& get_buffer() { return buffer_; }
};

}
}

template <typename StreamType>
void mysql::detail::execute_query(
	channel<StreamType>& channel,
	std::string_view query,
	resultset<StreamType>& output,
	error_code& err,
	error_info& info
)
{
	// Compose a com_query message, reset seq num
	query_processor<StreamType> processor (channel);
	processor.process_query_request(query);

	// Send it
	channel.write(boost::asio::buffer(processor.get_buffer()), err);
	if (err) return;

	// Read the response
	channel.read(processor.get_buffer(), err);
	if (err) return;

	// Response may be: ok_packet, err_packet, local infile request (not implemented), or response with fields
	auto num_fields = processor.process_query_response(output, err, info);
	if (!num_fields) // ok or err
	{
		return;
	}

	// We have a response with metadata, read all of the field definitions
	for (std::uint64_t i = 0; i < *num_fields; ++i)
	{
		// Read the field definition packet
		channel.read(processor.get_buffer(), err);
		if (err) return;

		// Process the message
		err = processor.process_field_definition();
		if (err) return;
	}

	// No EOF packet is expected here, as we require deprecate EOF capabilities
	output = std::move(processor).create_resultset();
	err.clear();
}


template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	void(mysql::error_code, mysql::error_info, mysql::resultset<StreamType>)
)
mysql::detail::async_execute_query(
	channel<StreamType>& chan,
	std::string_view query,
	CompletionToken&& token
)
{
	using HandlerSignature = void(error_code, error_info, resultset<StreamType>);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using BaseType = boost::beast::async_base<HandlerType, typename StreamType::executor_type>;
	using ResultsetType = resultset<StreamType>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op: BaseType, boost::asio::coroutine
	{
		std::shared_ptr<query_processor<StreamType>> processor_;
		std::uint64_t remaining_fields_ {0};

		Op(
			HandlerType&& handler,
			channel<StreamType>& channel,
			std::string_view query
		):
			BaseType(std::move(handler), channel.next_layer().get_executor()),
			processor_(std::make_shared<query_processor<StreamType>>(channel))
		{
			processor_->process_query_request(query);
		}

		// true => has fields (must continue reading)
		bool process_query_response(bool cont)
		{
			ResultsetType resultset;
			error_code err;
			error_info info;
			auto num_fields = processor_->process_query_response(resultset, err, info);
			if (!num_fields) // ok or err
			{
				this->complete(cont, err, std::move(info), std::move(resultset));
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
			this->complete(
				cont,
				error_code(),
				error_info(),
				std::move(*processor_).create_resultset()
			);
		}

		void operator()(
			error_code err,
			bool cont=true
		)
		{
			reenter(*this)
			{
				// The request message has already been composed in the ctor. Send it
				yield processor_->get_channel().async_write(
					boost::asio::buffer(processor_->get_buffer()),
					std::move(*this)
				);
				if (err)
				{
					this->complete(cont, err, error_info(), ResultsetType());
					yield break;
				}

				// Read the response
				yield processor_->get_channel().async_read(
					processor_->get_buffer(),
					std::move(*this)
				);
				if (err)
				{
					this->complete(cont, err, error_info(), ResultsetType());
					yield break;
				}

				// Response may be: ok_packet, err_packet, local infile request (not implemented), or response with fields
				if (!process_query_response(cont))
				{
					// Not a response with fields. complete() already called
					yield break;
				}

				// Read all of the field definitions
				while (remaining_fields_ > 0)
				{
					// Read the field definition packet
					yield processor_->get_channel().async_read(
						processor_->get_buffer(),
						std::move(*this)
					);
					if (!err)
					{
						// Process the message
						err = processor_->process_field_definition();
					}

					if (err)
					{
						this->complete(cont, err, error_info(), ResultsetType());
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
		chan,
		query
	)(error_code(), false);
	return initiator.result.get();
}

#include <boost/asio/unyield.hpp>


#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_EXECUTE_QUERY_IPP_ */
