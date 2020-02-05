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
	channel.write(buff, err);
	if (err) return;

	// Read response
	channel.read(buff, err);
	if (err) return;

	// Deserialize response
	DeserializationContext ctx (buff, channel.current_capabilities());
	std::uint8_t msg_type = 0;
	std::tie(err, msg_type) = deserialize_message_type(ctx);
	if (err) return;
	if (msg_type == 0)
	{
		com_stmt_prepare_ok_packet response;
		err = deserialize_message(response, ctx);
		if (err) return;
		output = prepared_statement<StreamType>(response);
	}
	else if (msg_type == error_packet_header)
	{
		err = process_error_packet(ctx, info);
	}
	else
	{
		err = make_error_code(Error::protocol_value_error);
	}
}



#endif /* INCLUDE_MYSQL_IMPL_NETWORK_ALGORITHMS_PREPARE_STATEMENT_IPP_ */
