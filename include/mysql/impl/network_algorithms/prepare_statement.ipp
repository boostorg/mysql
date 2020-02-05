#ifndef INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_IPP_
#define INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_IPP_

template <typename StreamType>
void mysql::detail::prepare_statement(
	channel<StreamType>& channel,
	std::string_view statement,
	error_code& err,
	error_info& info,
	prepared_statement<StreamType>& output
)
{
	// Prepare message
	com_stmt_prepare_packet packet { string_eof(statement) };
	bytestring buff;
	serialize_message(packet, channel.current_capabilities(), buff);

	// Write message
	channel.reset_sequence_number();
	channel.write(boost::asio::buffer(buff), err);
	if (err) return;

	// Read response
	channel.read(buff, err);
	if (err) return;

	// Deserialize response
	DeserializationContext ctx (boost::asio::buffer(buff), channel.current_capabilities());
	std::uint8_t msg_type = 0;
	std::tie(err, msg_type) = deserialize_message_type(ctx);
	if (err) return;

	if (msg_type == error_packet_header)
	{
		err = process_error_packet(ctx, info);
		return;
	}
	else if (msg_type != 0)
	{
		err = make_error_code(Error::protocol_value_error);
		return;
	}

	com_stmt_prepare_ok_packet response;
	err = deserialize_message(response, ctx);
	if (err) return;

	// Server sends now one packet per parameter and field.
	// We ignore these for now. TODO: do sth useful with these
	for (unsigned i = 0; i < response.num_columns.value + response.num_params.value; ++i)
	{
		channel.read(buff, err);
		if (err) return;
	}

	// Compose response
	output = prepared_statement<StreamType>(channel, response);
}



#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_IPP_ */
