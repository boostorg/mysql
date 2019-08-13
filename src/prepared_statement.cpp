
#include "prepared_statement.hpp"
#include "message_serialization.hpp"
#include "null_bitmap.hpp"

using namespace std;

static void read_param(mysql::MysqlStream& stream, mysql::ParamDefinition& output)
{
	stream.read(output.packet);
	mysql::deserialize(output.packet, output.value);
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

mysql::BinaryResultsetRow::BinaryResultsetRow(
	const std::vector<ParamDefinition>& fields,
	std::vector<std::uint8_t>&& packet
) :
	fields_ {fields},
	packet_ {std::move(packet)}
{
	values_.reserve(fields_.size());
	StmtExecuteNullBitmapTraits traits {fields_.size()};
	ReadIterator null_bitmap_first = packet_.data() + 1; // Skip header
	ReadIterator first = null_bitmap_first + traits.byte_count();
	ReadIterator last = packet_.data() + packet_.size();

	for (std::size_t i = 0; i < fields_.size(); ++i)
	{
		if (traits.is_null(null_bitmap_first, i))
			values_.emplace_back(nullptr);
		else
			values_.push_back(deserialize_field(fields_[i].value.type, first, last));
	}
	if (first != last)
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
	std::vector<ParamDefinition> params (response.num_params);
	for (int2 i = 0; i < response.num_params; ++i)
		read_param(stream, params[i]);
	std::vector<ParamDefinition> columns (response.num_columns);
	for (int2 i = 0; i < response.num_columns; ++i)
		read_param(stream, columns[i]);

	return PreparedStatement {stream, response.statement_id, move(params), move(columns)};
}

std::vector<mysql::BinaryResultsetRow> mysql::PreparedStatement::do_execute(const StmtExecute& message)
{
	// TODO: other cursor types
	DynamicBuffer write_buffer;
	serialize(write_buffer, message);
	stream_->reset_sequence_number();
	stream_->write(write_buffer.get());

	// Execute response header
	std::vector<std::uint8_t> read_buffer;
	stream_->read(read_buffer);
	StmtExecuteResponseHeader response_header;
	deserialize(read_buffer, response_header);

	// Read the parameters. Ignore the packets
	for (int1 i = 0; i < response_header.num_fields; ++i)
		stream_->read(read_buffer);

	// Read the result
	std::vector<mysql::BinaryResultsetRow> res;

	while (true)
	{
		read_buffer.clear();
		stream_->read(read_buffer);
		auto msg_type = get_message_type(read_buffer);
		if (msg_type == eof_packet_header)
			break;
		res.emplace_back(columns_, std::move(read_buffer));
	}

	return res;
}
