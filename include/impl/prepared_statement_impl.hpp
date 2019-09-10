#ifndef INCLUDE_IMPL_PREPARED_STATEMENT_IMPL_HPP_
#define INCLUDE_IMPL_PREPARED_STATEMENT_IMPL_HPP_

#include "message_serialization.hpp"
#include "mysql_stream.hpp"
#include "null_bitmap.hpp"
#include <cassert>

namespace mysql
{
namespace detail
{

inline void fill_execute_msg_impl(std::vector<BinaryValue>::iterator) {}

template <typename Param0, typename... Params>
void fill_execute_msg_impl(
	std::vector<BinaryValue>::iterator param_output,
	Param0&& param,
	Params&&... tail
)
{
	*param_output = std::forward<Param0>(param);
	fill_execute_msg_impl(std::next(param_output), std::forward<Params>(tail)...);
}


template <typename... Args>
void fill_execute_msg(StmtExecute& output, std::size_t num_params, Args&&... args)
{
	if (sizeof...(args) != num_params)
	{
		throw std::out_of_range {"Wrong number of parameters passed to prepared statement"};
	}
	output.num_params = static_cast<int1>(num_params);
	output.new_params_bind_flag = 1;
	output.param_values.resize(num_params);
	fill_execute_msg_impl(output.param_values.begin(), std::forward<Args>(args)...);
}

template <typename AsyncStream>
std::vector<ParamDefinition> read_fields(
	MysqlStream<AsyncStream>& stream,
	std::size_t quantity
)
{
	std::vector<ParamDefinition> res (quantity);
	for (auto& elm: res)
	{
		stream.read(elm.packet);
		deserialize(elm.packet, elm.value);
	}
	return res;
}

template <typename T>
BinaryValue deserialize_field_impl(
	ReadIterator& first,
	ReadIterator last
)
{
	T value;
	first = deserialize(first, last, value);
	return BinaryValue {value};
}

inline BinaryValue not_implemented()
{
	throw std::runtime_error{"Not implemented"};
}

inline BinaryValue deserialize_field(
	FieldType type,
	ReadIterator& first,
	ReadIterator last
)
{
	switch (type)
	{
    case FieldType::DECIMAL:
    case FieldType::VARCHAR:
    case FieldType::BIT:
    case FieldType::NEWDECIMAL:
    case FieldType::ENUM:
    case FieldType::SET:
    case FieldType::TINY_BLOB:
    case FieldType::MEDIUM_BLOB:
    case FieldType::LONG_BLOB:
    case FieldType::BLOB:
    case FieldType::VAR_STRING:
    case FieldType::STRING:
    case FieldType::GEOMETRY:
    	return deserialize_field_impl<string_lenenc>(first, last);
    case FieldType::TINY:
    	return deserialize_field_impl<int1>(first, last);
    case FieldType::SHORT:
    	return deserialize_field_impl<int2>(first, last);
    case FieldType::INT24:
    case FieldType::LONG:
    	return deserialize_field_impl<int4>(first, last);
    case FieldType::LONGLONG:
    	return deserialize_field_impl<int8>(first, last);
    case FieldType::FLOAT:
    	return not_implemented();
    case FieldType::DOUBLE:
    	return not_implemented();
    case FieldType::NULL_:
    	return deserialize_field_impl<nullptr_t>(first, last);
    case FieldType::TIMESTAMP:
    case FieldType::DATE:
    case FieldType::TIME:
    case FieldType::DATETIME:
    case FieldType::YEAR:
    default:
    	return not_implemented();
	}
}

inline void deserialize_binary_row(
	const std::vector<std::uint8_t>& packet,
	const std::vector<ParamDefinition>& fields,
	std::vector<BinaryValue>& output
)
{
	output.clear();
	output.reserve(fields.size());
	ResultsetRowNullBitmapTraits traits {fields.size()};
	ReadIterator null_bitmap_first = packet.data() + 1; // skip header
	ReadIterator current = null_bitmap_first + traits.byte_count();
	ReadIterator last = packet.data() + packet.size();

	for (std::size_t i = 0; i < fields.size(); ++i)
	{
		if (traits.is_null(null_bitmap_first, i))
			output.emplace_back(nullptr);
		else
			output.push_back(deserialize_field(fields[i].value.type, current, last));
	}
	if (current != last)
	{
		throw std::out_of_range {"Leftover data after binary row"};
	}
}

} // detail
} // mysql


template <typename AsyncStream>
mysql::PreparedStatement<AsyncStream>::PreparedStatement(
	MysqlStream<AsyncStream>& stream,
	int4 statement_id,
	std::vector<ParamDefinition>&& params,
	std::vector<ParamDefinition>&& columns
) :
	stream_ {&stream},
	statement_id_ {statement_id},
	params_ {std::move(params)},
	columns_ {std::move(columns)}
{
};

template <typename AsyncStream>
mysql::PreparedStatement<AsyncStream> mysql::PreparedStatement<AsyncStream>::prepare(
	MysqlStream<AsyncStream>& stream,
	std::string_view query
)
{
	// Write the prepare request
	StmtPrepare packet {{query}};
	DynamicBuffer write_buffer;
	serialize(write_buffer, packet);
	stream.reset_sequence_number();
	stream.write(write_buffer.get());

	// Get the prepare response
	std::vector<std::uint8_t> read_buffer;
	stream.read(read_buffer);
	int1 status = get_message_type(read_buffer);
	if (status != 0)
		throw std::runtime_error {"Error preparing statement" + std::string{query}};
	StmtPrepareResponseHeader response;
	deserialize(read_buffer.data() + 1, read_buffer.data() + read_buffer.size(), response);

	// Read the parameters and columns if any
	auto params = detail::read_fields(stream, response.num_params);
	auto fields = detail::read_fields(stream, response.num_columns);

	return PreparedStatement<AsyncStream> {
		stream,
		response.statement_id,
		std::move(params),
		std::move(fields)
	};
}

template <typename AsyncStream>
template <typename... Params>
mysql::BinaryResultset<AsyncStream> mysql::PreparedStatement<AsyncStream>::execute_with_cursor(
	int4 fetch_count,
	Params&&... actual_params
)
{
	int1 flags = fetch_count == MAX_FETCH_COUNT ? CURSOR_TYPE_NO_CURSOR : CURSOR_TYPE_READ_ONLY;
	StmtExecute message
	{
		statement_id_,
		flags
	};
	detail::fill_execute_msg(message, params_.size(), std::forward<Params>(actual_params)...);
	return do_execute(message, fetch_count);
}

template <typename AsyncStream>
void mysql::BinaryResultset<AsyncStream>::read_metadata()
{
	stream_->read(current_packet_);
	if (get_message_type(current_packet_) == ok_packet_header) // Implicitly checks for errors
	{
		process_ok();
	}
	else
	{
		// Header containing number of fields
		StmtExecuteResponseHeader response_header;
		deserialize(current_packet_, response_header);

		// Fields
		fields_ = detail::read_fields(*stream_, response_header.num_fields);

		// First row
		retrieve_next();
	}
}

template <typename AsyncStream>
void mysql::BinaryResultset<AsyncStream>::process_ok()
{
	deserialize(current_packet_.data() + 1,
			current_packet_.data() + current_packet_.size(),
			ok_packet_);
	if (cursor_exists() &&
		!(ok_packet_.status_flags & SERVER_STATUS_LAST_ROW_SENT))
	{
		send_fetch();
		retrieve_next();
	}
	else
	{
		state_ = State::exhausted;
	}
}

template <typename AsyncStream>
void mysql::BinaryResultset<AsyncStream>::send_fetch()
{
	mysql::StmtFetch msg { statement_id_, fetch_count_ };
	DynamicBuffer buffer;
	serialize(buffer, msg);
	stream_->reset_sequence_number();
	stream_->write(buffer.get());
}

template <typename AsyncStream>
bool mysql::BinaryResultset<AsyncStream>::retrieve_next()
{
	if (state_ == State::exhausted)
		return false;

	stream_->read(current_packet_);
	auto msg_type = get_message_type(current_packet_);
	if (msg_type == eof_packet_header)
	{
		process_ok();
	}
	else
	{
		detail::deserialize_binary_row(current_packet_, fields_, current_values_);
		state_ = State::data_available;
	}
	return more_data();
}

template <typename AsyncStream>
const mysql::OkPacket& mysql::BinaryResultset<AsyncStream>::ok_packet() const
{
	assert(state_ == State::exhausted ||
			(state_ == State::data_available && cursor_exists()));
	return ok_packet_;
}

template <typename AsyncStream>
const std::vector<mysql::BinaryValue>& mysql::BinaryResultset<AsyncStream>::values() const
{
	assert(state_ == State::data_available);
	return current_values_;
}

template <typename AsyncStream>
mysql::BinaryResultset<AsyncStream> mysql::PreparedStatement<AsyncStream>::do_execute(
	const StmtExecute& message,
	int4 fetch_count
)
{
	std::vector<std::uint8_t> read_buffer;

	DynamicBuffer write_buffer;
	serialize(write_buffer, message);
	stream_->reset_sequence_number();
	stream_->write(write_buffer.get());

	return mysql::BinaryResultset {*stream_, statement_id_, fetch_count};
}

template <typename AsyncStream>
void mysql::PreparedStatement<AsyncStream>::close()
{
	assert(statement_id_ != 0);
	StmtClose msg { statement_id_ };

	DynamicBuffer write_buffer;
	serialize(write_buffer, msg);
	stream_->reset_sequence_number();
	stream_->write(write_buffer.get());
}

#endif
