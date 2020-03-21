#ifndef INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_COMMON_MESSAGES_IPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_COMMON_MESSAGES_IPP_

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
	boost::mysql::detail::ok_packet,
	boost::mysql::detail::struct_tag
>::deserialize_(
	ok_packet& output,
	deserialization_context& ctx
) noexcept
{
	{
		auto err = deserialize_fields(
			ctx,
			output.affected_rows,
			output.last_insert_id,
			output.status_flags,
			output.warnings
		);
		if (err == errc::ok && ctx.enough_size(1)) // message is optional, may be omitted
		{
			err = deserialize(output.info, ctx);
		}
		return err;
	}
}

inline boost::mysql::errc
boost::mysql::detail::serialization_traits<
	boost::mysql::detail::column_definition_packet,
	boost::mysql::detail::struct_tag
>::deserialize_(
	column_definition_packet& output,
	deserialization_context& ctx
) noexcept
{
	int_lenenc length_of_fixed_fields;
	int2 final_padding;
	return deserialize_fields(
		ctx,
		output.catalog,
		output.schema,
		output.table,
		output.org_table,
		output.name,
		output.org_name,
		length_of_fixed_fields,
		output.character_set,
		output.column_length,
		output.type,
		output.flags,
		output.decimals,
		final_padding
	);
}

inline boost::mysql::error_code boost::mysql::detail::process_error_packet(
	deserialization_context& ctx,
	error_info& info
)
{
	err_packet error_packet;
	auto code = deserialize_message(error_packet, ctx);
	if (code) return code;
	info.set_message(std::string(error_packet.error_message.value));
	return make_error_code(static_cast<errc>(error_packet.error_code.value));
}


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_PROTOCOL_IMPL_COMMON_MESSAGES_IPP_ */
