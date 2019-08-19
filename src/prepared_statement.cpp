
#include "prepared_statement.hpp"
#include "message_serialization.hpp"
#include "null_bitmap.hpp"
#include <cassert>

using namespace std;

static std::vector<mysql::ParamDefinition> read_fields(
	mysql::MysqlStream& stream,
	std::size_t quantity
)
{
	std::vector<mysql::ParamDefinition> res (quantity);
	for (auto& elm: res)
	{
		stream.read(elm.packet);
		mysql::deserialize(elm.packet, elm.value);
	}
	return res;
}

template <typename T>
static mysql::BinaryValue deserialize_field_impl(
	mysql::ReadIterator& first,
	mysql::ReadIterator last
)
{
	T value;
	first = mysql::deserialize(first, last, value);
	return mysql::BinaryValue {value};
}

mysql::BinaryValue not_implemented()
{
	throw std::runtime_error{"Not implemented"};
}

static mysql::BinaryValue deserialize_field(
	mysql::FieldType type,
	mysql::ReadIterator& first,
	mysql::ReadIterator last
)
{
	switch (type)
	{
    case mysql::FieldType::DECIMAL:
    case mysql::FieldType::VARCHAR:
    case mysql::FieldType::BIT:
    case mysql::FieldType::NEWDECIMAL:
    case mysql::FieldType::ENUM:
    case mysql::FieldType::SET:
    case mysql::FieldType::TINY_BLOB:
    case mysql::FieldType::MEDIUM_BLOB:
    case mysql::FieldType::LONG_BLOB:
    case mysql::FieldType::BLOB:
    case mysql::FieldType::VAR_STRING:
    case mysql::FieldType::STRING:
    case mysql::FieldType::GEOMETRY:
    	return deserialize_field_impl<mysql::string_lenenc>(first, last);
    case mysql::FieldType::TINY:
    	return deserialize_field_impl<mysql::int1>(first, last);
    case mysql::FieldType::SHORT:
    	return deserialize_field_impl<mysql::int2>(first, last);
    case mysql::FieldType::INT24:
    case mysql::FieldType::LONG:
    	return deserialize_field_impl<mysql::int4>(first, last);
    case mysql::FieldType::LONGLONG:
    	return deserialize_field_impl<mysql::int8>(first, last);
    case mysql::FieldType::FLOAT:
    	return not_implemented();
    case mysql::FieldType::DOUBLE:
    	return not_implemented();
    case mysql::FieldType::NULL_:
    	return deserialize_field_impl<nullptr_t>(first, last);
    case mysql::FieldType::TIMESTAMP:
    case mysql::FieldType::DATE:
    case mysql::FieldType::TIME:
    case mysql::FieldType::DATETIME:
    case mysql::FieldType::YEAR:
    default:
    	return not_implemented();
	}
}

static void deserialize_binary_row(
	const std::vector<std::uint8_t>& packet,
	const std::vector<mysql::ParamDefinition>& fields,
	std::vector<mysql::BinaryValue>& output
)
{
	output.clear();
	output.reserve(fields.size());
	mysql::ResultsetRowNullBitmapTraits traits {fields.size()};
	mysql::ReadIterator null_bitmap_first = packet.data() + 1; // skip header
	mysql::ReadIterator current = null_bitmap_first + traits.byte_count();
	mysql::ReadIterator last = packet.data() + packet.size();

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

mysql::PreparedStatement::PreparedStatement(
	MysqlStream& stream,
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

mysql::PreparedStatement mysql::PreparedStatement::prepare(MysqlStream& stream, std::string_view query)
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
	auto params = read_fields(stream, response.num_params);
	auto fields = read_fields(stream, response.num_columns);

	return PreparedStatement {stream, response.statement_id, move(params), move(fields)};
}

void mysql::BinaryResultset::read_metadata()
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
		fields_ = read_fields(*stream_, response_header.num_fields);

		// First row
		retrieve_next();
	}
}

void mysql::BinaryResultset::process_ok()
{
	deserialize(current_packet_.data() + 1,
			current_packet_.data() + current_packet_.size(),
			ok_packet_);
	if (ok_packet_.status_flags & SERVER_STATUS_CURSOR_EXISTS)
	{
		// TODO: handle cursor semantics
	}
	state_ = State::exhausted;
}

bool mysql::BinaryResultset::retrieve_next()
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
		deserialize_binary_row(current_packet_, fields_, current_values_);
		state_ = State::data_available;
	}
	return more_data();
}

const mysql::OkPacket& mysql::BinaryResultset::ok_packet() const
{
	assert(state_ == State::exhausted);
	return ok_packet_;
}

const std::vector<mysql::BinaryValue>& mysql::BinaryResultset::values() const
{
	assert(state_ == State::data_available);
	return current_values_;
}

mysql::BinaryResultset mysql::PreparedStatement::do_execute(const StmtExecute& message)
{
	std::vector<std::uint8_t> read_buffer;

	// TODO: other cursor types
	DynamicBuffer write_buffer;
	serialize(write_buffer, message);
	stream_->reset_sequence_number();
	stream_->write(write_buffer.get());

	return mysql::BinaryResultset {*stream_};
}


