#ifndef INCLUDE_MYSQL_IMPL_MESSAGES_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_MESSAGES_IMPL_HPP_

#include "mysql/impl/basic_serialization.hpp"

inline mysql::Error mysql::detail::deserialize(
	msgs::ok_packet& output,
	DeserializationContext& ctx
) noexcept
{
	auto err = deserialize(output.affected_rows, ctx);
	if (err == Error::ok)
	{
		err = deserialize(output.last_insert_id, ctx);
	}
	if (err == Error::ok)
	{
		err = deserialize(output.status_flags, ctx);
	}
	if (err == Error::ok)
	{
		err = deserialize(output.warnings, ctx);
	}
	if (err == Error::ok && ctx.enough_size(1)) // message is optional, may be omitted
	{
		err = deserialize(output.info, ctx);
	}
	return err;
}


#endif /* INCLUDE_MYSQL_IMPL_MESSAGES_IMPL_HPP_ */
