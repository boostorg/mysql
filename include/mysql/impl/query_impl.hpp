#ifndef INCLUDE_MYSQL_IMPL_QUERY_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_QUERY_IMPL_HPP_

#include "mysql/impl/messages.hpp"
#include "mysql/impl/basic_serialization.hpp"
#include "mysql/impl/deserialize_row.hpp"

namespace mysql
{
namespace detail
{

template <typename ChannelType>
using channel_stream_type = typename ChannelType::stream_type;

}
}

template <typename ChannelType, typename Allocator>
void mysql::detail::execute_query(
	ChannelType& channel,
	std::string_view query,
	resultset<channel_stream_type<ChannelType>, Allocator>& output,
	error_code& err
)
{
	// Compose a com_query message
	msgs::com_query query_msg;
	query_msg.query.value = query;

	// Serialize it
	capabilities caps = channel.current_capabilities();
	bytestring<Allocator> buffer;
	serialize_message(query_msg, caps, buffer);

	// Send it
	channel.reset_sequence_number();
	channel.write(boost::asio::buffer(buffer), err);
	if (err) return;

	// Read the response
	channel.read(buffer, err);
	if (err) return;

	// Response may be: ok_packet, err_packet, local infile request (TODO)
	// If it is none of this, then the message type itself is the beginning of
	// a length-encoded int containing the field count
	DeserializationContext ctx (boost::asio::buffer(buffer), caps);
	std::uint8_t msg_type;
	err = deserialize_message_type(msg_type, ctx);
	if (err) return;
	if (msg_type == ok_packet_header)
	{
		msgs::ok_packet ok_packet;
		err = deserialize_message(ok_packet, ctx);
		if (err) return;
		output = resultset<channel_stream_type<ChannelType>, Allocator>(channel, std::move(buffer), ok_packet);
		err.clear();
		return;
	}
	else if (msg_type == error_packet_header)
	{
		err = process_error_packet(ctx);
		return;
	}

	// Resultset with metadata. First packet is an int_lenenc with
	// the number of field definitions to expect. Message type is part
	// of this packet, so we must rewind the context
	ctx.set_first(buffer.data());
	int_lenenc num_fields;
	err = deserialize_message(num_fields, ctx);
	if (err) return;

	std::vector<field_metadata> fields;
	std::vector<bytestring<Allocator>> field_buffers;
	fields.reserve(num_fields.value);
	field_buffers.reserve(num_fields.value);

	// Read all of the field definitions
	for (std::uint64_t i = 0; i < num_fields.value; ++i)
	{
		// Read the field definition packet
		bytestring<Allocator> field_definition_buffer;
		channel.read(field_definition_buffer, err);
		if (err) return;

		// Deserialize the message
		msgs::column_definition field_definition;
		ctx = DeserializationContext(boost::asio::buffer(field_definition_buffer), caps);
		err = deserialize_message(field_definition, ctx);
		if (err) return;

		// Add it to our array
		fields.push_back(field_definition);
		field_buffers.push_back(std::move(field_definition_buffer));
	}

	// No EOF packet is expected here, as we require deprecate EOF capabilities
	output = resultset<channel_stream_type<ChannelType>, Allocator>(
		channel,
		resultset_metadata<Allocator>(std::move(field_buffers), std::move(fields))
	);
	err.clear();
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




#endif /* INCLUDE_MYSQL_IMPL_QUERY_IMPL_HPP_ */
